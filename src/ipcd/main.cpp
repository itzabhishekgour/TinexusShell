#include "common/logger.hpp"
#include "common/version.hpp"
#include "ipcd/transport/unix_socket.hpp"
#include "ipcd/transport/epoll_loop.hpp"
#include "ipcd/security/peer_validator.hpp"
#include "ipcd/registry/service_registry.hpp"
#include "ipcd/broker/pubsub_broker.hpp"
#include "ipcd/protocol/header.hpp"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pwd.h>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <csignal>
#include <atomic>

namespace {

// ---------------------------------------------------------------------------
// Pending read state — per-client framing buffer for edge-triggered epoll
// ---------------------------------------------------------------------------
struct ClientState {
    std::vector<uint8_t> read_buf;  // accumulates bytes until a full message arrives
    bool header_complete{false};
    tinexus::ipcd::protocol::Header hdr{};
};

std::unordered_map<int, ClientState> g_client_states;
std::mutex g_state_mutex;

std::atomic<bool> g_running{true};

void signal_handler(int) { g_running = false; }

// ---------------------------------------------------------------------------
// send_frame — write a complete protocol frame (header + payload) to a fd
// Returns false on error.
// ---------------------------------------------------------------------------
bool send_frame(int fd,
                tinexus::ipcd::protocol::MessageType type,
                uint32_t sequence_id,
                const uint8_t* payload,
                uint32_t payload_len) {
    using namespace tinexus::ipcd::protocol;
    Header out_hdr{};
    out_hdr.magic       = TINEXUS_IPC_MAGIC;
    out_hdr.version     = TINEXUS_IPC_VERSION_1;
    out_hdr.msg_type    = static_cast<uint16_t>(type);
    out_hdr.flags       = 0;
    out_hdr.sequence_id = sequence_id;
    out_hdr.payload_len = payload_len;
    out_hdr.checksum    = 0; // CRC32 reserved for v1.1

    // Write header
    if (write(fd, &out_hdr, sizeof(out_hdr)) != static_cast<ssize_t>(sizeof(out_hdr))) {
        return false;
    }
    // Write payload
    if (payload_len > 0 && payload != nullptr) {
        if (write(fd, payload, payload_len) != static_cast<ssize_t>(payload_len)) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// dispatch_message — full routing engine
// Called once a complete (header + payload) has been buffered.
// ---------------------------------------------------------------------------
void dispatch_message(int sender_fd,
                      const tinexus::ipcd::protocol::Header& hdr,
                      const std::vector<uint8_t>& payload) {
    using namespace tinexus::ipcd;
    using MT = protocol::MessageType;
    auto msg_type = static_cast<MT>(hdr.msg_type);

    switch (msg_type) {

    // -----------------------------------------------------------------------
    // SYS_REGISTER_SERVICE — payload: null-terminated service name string
    // -----------------------------------------------------------------------
    case MT::SYS_REGISTER_SERVICE: {
        std::string name(reinterpret_cast<const char*>(payload.data()),
                         payload.size());
        // Strip trailing null if present
        while (!name.empty() && name.back() == '\0') name.pop_back();

        bool ok = registry::ServiceRegistry::instance().register_service(name, sender_fd, hdr.flags);
        if (ok) {
            tinexus::log::info("[ipcd] Service registered: '{}' on fd={}", name, sender_fd);
        } else {
            tinexus::log::warn("[ipcd] Duplicate service registration attempt: '{}'", name);
        }
        // Acknowledge
        uint8_t ack = ok ? 1 : 0;
        send_frame(sender_fd, MT::SYS_SERVICE_EVENT, hdr.sequence_id, &ack, 1);
        break;
    }

    // -----------------------------------------------------------------------
    // SYS_UNREGISTER_SERVICE
    // -----------------------------------------------------------------------
    case MT::SYS_UNREGISTER_SERVICE: {
        std::string name(reinterpret_cast<const char*>(payload.data()), payload.size());
        while (!name.empty() && name.back() == '\0') name.pop_back();
        registry::ServiceRegistry::instance().unregister_service(name);
        tinexus::log::info("[ipcd] Service unregistered: '{}'", name);
        break;
    }

    // -----------------------------------------------------------------------
    // SYS_DISCOVER_SERVICE — payload: service name to look up
    // Reply: provider_fd as little-endian int32 (-1 if not found)
    // -----------------------------------------------------------------------
    case MT::SYS_DISCOVER_SERVICE: {
        std::string name(reinterpret_cast<const char*>(payload.data()), payload.size());
        while (!name.empty() && name.back() == '\0') name.pop_back();
        auto svc = registry::ServiceRegistry::instance().lookup_service(name);
        int32_t result_fd = svc.has_value() ? svc->provider_fd : -1;
        send_frame(sender_fd, MT::SYS_SERVICE_EVENT, hdr.sequence_id,
                   reinterpret_cast<const uint8_t*>(&result_fd), sizeof(result_fd));
        break;
    }

    // -----------------------------------------------------------------------
    // SYS_SUBSCRIBE_TOPIC — payload: uint16_t topic id
    // -----------------------------------------------------------------------
    case MT::SYS_SUBSCRIBE_TOPIC: {
        if (payload.size() >= 2) {
            uint16_t topic;
            std::memcpy(&topic, payload.data(), sizeof(topic));
            broker::PubSubBroker::instance().subscribe(topic, sender_fd);
            tinexus::log::debug("[ipcd] fd={} subscribed to topic {}", sender_fd, topic);
        }
        break;
    }

    // -----------------------------------------------------------------------
    // SYS_UNSUBSCRIBE_TOPIC
    // -----------------------------------------------------------------------
    case MT::SYS_UNSUBSCRIBE_TOPIC: {
        if (payload.size() >= 2) {
            uint16_t topic;
            std::memcpy(&topic, payload.data(), sizeof(topic));
            broker::PubSubBroker::instance().unsubscribe(topic, sender_fd);
        }
        break;
    }

    // -----------------------------------------------------------------------
    // SYS_PING — echo back a SYS_PONG
    // -----------------------------------------------------------------------
    case MT::SYS_PING: {
        send_frame(sender_fd, MT::SYS_PONG, hdr.sequence_id, nullptr, 0);
        tinexus::log::debug("[ipcd] PING → PONG to fd={}", sender_fd);
        break;
    }

    // -----------------------------------------------------------------------
    // SEARCH_QUERY — route to the registered "searchd" service
    // If searchd is not registered, reply with empty SEARCH_RESULT.
    // -----------------------------------------------------------------------
    case MT::SEARCH_QUERY: {
        auto svc = registry::ServiceRegistry::instance().lookup_service("searchd");
        if (svc.has_value() && svc->provider_fd != -1) {
            // Forward verbatim: searchd sees the same header + payload,
            // but the sequence_id encodes the requester fd so searchd
            // can route the reply back via ipcd.
            // We embed sender_fd in the high 16 bits of sequence_id.
            // (safe: fd values are small on Linux)
            tinexus::ipcd::protocol::Header fwd_hdr = hdr;
            // Store sender fd in flags field so searchd reply carries it back
            fwd_hdr.flags = static_cast<uint16_t>(sender_fd & 0xFFFF);

            if (write(svc->provider_fd, &fwd_hdr, sizeof(fwd_hdr)) == sizeof(fwd_hdr)) {
                if (!payload.empty()) {
                    if (write(svc->provider_fd, payload.data(), payload.size())
                        != static_cast<ssize_t>(payload.size())) {
                        tinexus::log::warn("[ipcd] Failed to forward search query payload");
                    }
                }
                tinexus::log::debug("[ipcd] SEARCH_QUERY forwarded to searchd fd={}", svc->provider_fd);
            } else {
                tinexus::log::warn("[ipcd] searchd fd write failed — service may have died");
                // Reply with empty result set so launcher doesn't hang
                send_frame(sender_fd, MT::SEARCH_RESULT, hdr.sequence_id, nullptr, 0);
            }
        } else {
            tinexus::log::warn("[ipcd] SEARCH_QUERY received but 'searchd' not registered");
            send_frame(sender_fd, MT::SEARCH_RESULT, hdr.sequence_id, nullptr, 0);
        }
        break;
    }

    // -----------------------------------------------------------------------
    // SEARCH_RESULT — reply from searchd, route back to original requester.
    // The requester fd is stored in hdr.flags by the forwarding step above.
    // -----------------------------------------------------------------------
    case MT::SEARCH_RESULT: {
        int requester_fd = static_cast<int>(hdr.flags);
        if (requester_fd > 0) {
            send_frame(requester_fd, MT::SEARCH_RESULT, hdr.sequence_id,
                       payload.empty() ? nullptr : payload.data(),
                       static_cast<uint32_t>(payload.size()));
            tinexus::log::debug("[ipcd] SEARCH_RESULT routed back to fd={} ({} bytes payload)",
                                requester_fd, payload.size());
        }
        break;
    }

    // -----------------------------------------------------------------------
    // SHORTCUT_ACTIVATED — broadcast to all subscribers of LAUNCHER_OPEN topic
    // -----------------------------------------------------------------------
    case MT::SHORTCUT_ACTIVATED: {
        auto subscribers = broker::PubSubBroker::instance()
            .get_subscribers(static_cast<uint16_t>(MT::LAUNCHER_OPEN));
        for (int sub_fd : subscribers) {
            send_frame(sub_fd, MT::LAUNCHER_SHOW, hdr.sequence_id,
                       payload.empty() ? nullptr : payload.data(),
                       static_cast<uint32_t>(payload.size()));
        }
        tinexus::log::debug("[ipcd] SHORTCUT_ACTIVATED broadcast to {} launcher(s)",
                            subscribers.size());
        break;
    }

    // -----------------------------------------------------------------------
    // CONFIG_CHANGED / THEME_CHANGED — broadcast to all topic subscribers
    // -----------------------------------------------------------------------
    case MT::CONFIG_CHANGED:
    case MT::THEME_CHANGED: {
        auto subscribers = broker::PubSubBroker::instance()
            .get_subscribers(hdr.msg_type);
        for (int sub_fd : subscribers) {
            send_frame(sub_fd, msg_type, hdr.sequence_id,
                       payload.empty() ? nullptr : payload.data(),
                       static_cast<uint32_t>(payload.size()));
        }
        tinexus::log::debug("[ipcd] Broadcast msg_type={} to {} subscribers",
                            hdr.msg_type, subscribers.size());
        break;
    }

    // -----------------------------------------------------------------------
    // SYS_SHUTDOWN — graceful shutdown signal
    // -----------------------------------------------------------------------
    case MT::SYS_SHUTDOWN:
        tinexus::log::info("[ipcd] SYS_SHUTDOWN received — shutting down broker");
        g_running = false;
        break;

    default:
        tinexus::log::debug("[ipcd] Unhandled msg_type={} from fd={}", hdr.msg_type, sender_fd);
        break;
    }
}

// ---------------------------------------------------------------------------
// handle_client_data — edge-triggered read: drain socket into buffer,
// dispatch complete messages as they accumulate.
// ---------------------------------------------------------------------------
void handle_client_data(int fd,
                        tinexus::ipcd::transport::EpollLoop& loop) {
    using namespace tinexus::ipcd::protocol;
    std::lock_guard<std::mutex> lock(g_state_mutex);
    auto& state = g_client_states[fd];

    // Edge-triggered: read until EAGAIN
    while (true) {
        uint8_t tmp[4096];
        ssize_t n = read(fd, tmp, sizeof(tmp));
        if (n > 0) {
            state.read_buf.insert(state.read_buf.end(), tmp, tmp + n);
        } else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
            // Connection closed or error
            tinexus::log::debug("[ipcd] Client fd={} disconnected", fd);
            tinexus::ipcd::registry::ServiceRegistry::instance().unregister_by_fd(fd);
            tinexus::ipcd::broker::PubSubBroker::instance().unsubscribe_all(fd);
            loop.unregister_fd(fd);
            g_client_states.erase(fd);
            close(fd);
            return;
        } else {
            break; // EAGAIN — no more data
        }
    }

    // Process all complete messages in the buffer
    constexpr size_t HDR_SIZE = sizeof(Header);
    while (true) {
        if (!state.header_complete) {
            if (state.read_buf.size() < HDR_SIZE) break; // wait for full header
            std::memcpy(&state.hdr, state.read_buf.data(), HDR_SIZE);

            // Validate magic + version
            if (state.hdr.magic != TINEXUS_IPC_MAGIC ||
                state.hdr.version != TINEXUS_IPC_VERSION_1) {
                tinexus::log::warn("[ipcd] Bad magic/version from fd={}, closing", fd);
                loop.unregister_fd(fd);
                g_client_states.erase(fd);
                close(fd);
                return;
            }

            // Guard against absurdly large payloads (4 MB limit)
            if (state.hdr.payload_len > 4u * 1024u * 1024u) {
                tinexus::log::warn("[ipcd] Payload too large ({} bytes) from fd={}",
                                   state.hdr.payload_len, fd);
                loop.unregister_fd(fd);
                g_client_states.erase(fd);
                close(fd);
                return;
            }

            state.read_buf.erase(state.read_buf.begin(),
                                 state.read_buf.begin() + static_cast<ptrdiff_t>(HDR_SIZE));
            state.header_complete = true;
        }

        // Wait for full payload
        if (state.read_buf.size() < state.hdr.payload_len) break;

        std::vector<uint8_t> payload(state.read_buf.begin(),
                                     state.read_buf.begin() + state.hdr.payload_len);
        state.read_buf.erase(state.read_buf.begin(),
                             state.read_buf.begin() + state.hdr.payload_len);
        state.header_complete = false;

        // Dispatch outside the lock to avoid deadlocks if dispatch sends back
        dispatch_message(fd, state.hdr, payload);
    }
}

} // anonymous namespace

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    tinexus::log::set_component_name("tinexus-ipcd");
    tinexus::log::info("Starting tinexus-ipcd v{} — Full IPC Broker & Message Dispatch Engine",
                       tinexus::VERSION_STRING);

    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGPIPE, SIG_IGN); // Never crash on broken pipe

    uid_t uid = getuid();
    std::string socket_path = "/run/user/" + std::to_string(uid) + "/tinexus/ipc.sock";

    tinexus::ipcd::transport::UnixSocketServer server(socket_path);
    if (!server.bind_and_listen()) {
        tinexus::log::error("[ipcd] Fatal: Failed to start IPC server at {}", socket_path);
        return 1;
    }

    tinexus::ipcd::transport::EpollLoop loop;
    if (!loop.register_fd(server.get_fd(), EPOLLIN, (void*)(intptr_t)server.get_fd())) {
        tinexus::log::error("[ipcd] Fatal: Failed to register server fd with epoll");
        return 1;
    }

    tinexus::log::info("[ipcd] Broker ready at {}  (magic=0x{:08X}, proto=v{}.{})",
                       socket_path,
                       tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC,
                       (tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1 >> 8) & 0xFF,
                       tinexus::ipcd::protocol::TINEXUS_IPC_VERSION_1 & 0xFF);

    loop.run([&server, &loop](const epoll_event& ev) {
        if (!g_running) {
            loop.stop();
            return;
        }

        int fd = (int)(intptr_t)ev.data.ptr;

        if (fd == server.get_fd()) {
            // Accept new connections (may arrive in burst on edge-triggered)
            while (true) {
                int client_fd = accept4(server.get_fd(), nullptr, nullptr,
                                        SOCK_NONBLOCK | SOCK_CLOEXEC);
                if (client_fd == -1) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        tinexus::log::error("[ipcd] accept4 failed: errno={}", errno);
                    }
                    break;
                }

                auto peer = tinexus::ipcd::security::PeerValidator::get_peer_identity(client_fd);
                if (peer && tinexus::ipcd::security::PeerValidator::is_authorized(*peer)) {
                    tinexus::log::info("[ipcd] New client: PID={} UID={} exe={}",
                                      peer->pid, peer->uid, peer->executable_path);
                    loop.register_fd(client_fd, EPOLLIN | EPOLLRDHUP,
                                     (void*)(intptr_t)client_fd);
                    // Initialise per-client framing state
                    std::lock_guard<std::mutex> lock(g_state_mutex);
                    g_client_states[client_fd] = ClientState{};
                } else {
                    tinexus::log::warn("[ipcd] Rejected unauthorized connection");
                    close(client_fd);
                }
            }
        } else {
            // Disconnection / error
            if (ev.events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                tinexus::log::debug("[ipcd] Client fd={} hang-up", fd);
                tinexus::ipcd::registry::ServiceRegistry::instance().unregister_by_fd(fd);
                tinexus::ipcd::broker::PubSubBroker::instance().unsubscribe_all(fd);
                loop.unregister_fd(fd);
                std::lock_guard<std::mutex> lock(g_state_mutex);
                g_client_states.erase(fd);
                close(fd);
            } else if (ev.events & EPOLLIN) {
                // Full framing + dispatch
                handle_client_data(fd, loop);
            }
        }
    });

    tinexus::log::info("[ipcd] Broker shutdown complete.");
    return 0;
}
