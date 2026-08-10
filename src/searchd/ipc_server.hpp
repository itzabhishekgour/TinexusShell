// searchd/ipc_server.hpp
// ----------------------------------------------------------------------------
// IpcServer — searchd's socket connection to tinexus-ipcd.
//
// At startup, searchd connects to ipcd, registers itself as "searchd",
// then enters a read loop: for every SEARCH_QUERY it executes the full
// multi-provider ranking pipeline and writes a SEARCH_RESULT reply.
//
// Threading: IpcServer runs its read loop in its own std::thread.
// All provider calls happen on that thread (no additional threads needed
// because provider execution is already fast: ≤30ms target on T2 hardware).
// ----------------------------------------------------------------------------
#ifndef TINEXUS_SEARCHD_IPC_SERVER_HPP
#define TINEXUS_SEARCHD_IPC_SERVER_HPP

#include "searchd/provider_manager.hpp"
#include "searchd/ranking_stage.hpp"
#include "searchd/normalizer.hpp"
#include "searchd/cache.hpp"
#include "searchd/session.hpp"
#include "searchd/wire_protocol.hpp"
#include "common/logger.hpp"

// ipcd protocol types (header-only, no link dependency)
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

// POSIX
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

namespace tinexus::searchd {

// ============================================================================
// Protocol constants (must stay in sync with ipcd/protocol/header.hpp)
// ============================================================================
namespace proto {
    constexpr uint32_t MAGIC   = 0x544E5853; // "TNXS"
    constexpr uint16_t VER_1   = 0x0100;

    enum class MsgType : uint16_t {
        SYS_REGISTER_SERVICE  = 0,
        SYS_SERVICE_EVENT     = 5,
        SYS_PING              = 10,
        SYS_PONG              = 11,
        SEARCH_QUERY          = 2000,
        SEARCH_RESULT         = 2001,
    };

#pragma pack(push, 1)
    struct Header {
        uint32_t magic;
        uint16_t version;
        uint16_t msg_type;
        uint16_t flags;
        uint32_t sequence_id;
        uint32_t payload_len;
        uint32_t checksum;
    };
#pragma pack(pop)
    static_assert(sizeof(Header) == 22, "proto::Header must be 22 bytes");
} // namespace proto

// ============================================================================
// IpcServer
// ============================================================================
class IpcServer {
public:
    explicit IpcServer(ProviderManager& mgr,
                       RankingPipeline&  ranker,
                       SearchCache&      cache)
        : m_mgr(mgr), m_ranker(ranker), m_cache(cache) {}

    ~IpcServer() { stop(); }

    // Non-copyable, non-movable
    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    // -----------------------------------------------------------------------
    // start() — connect to ipcd, register, begin event loop in background thread
    // Returns true if connected successfully.
    // -----------------------------------------------------------------------
    bool start() {
        uid_t uid = getuid();
        m_socket_path = "/run/user/" + std::to_string(uid) + "/tinexus/ipc.sock";

        m_fd = connect_to_ipcd(m_socket_path);
        if (m_fd == -1) {
            log::warn("[searchd-ipc] ipcd not available at {} — running without IPC",
                      m_socket_path);
            return false;
        }

        // Register ourselves as "searchd"
        if (!register_service("searchd")) {
            log::error("[searchd-ipc] Failed to register with ipcd");
            close(m_fd);
            m_fd = -1;
            return false;
        }

        log::info("[searchd-ipc] Registered as 'searchd' with tinexus-ipcd");
        m_running = true;
        m_thread  = std::thread(&IpcServer::run_loop, this);
        return true;
    }

    void stop() {
        m_running = false;
        if (m_fd != -1) {
            shutdown(m_fd, SHUT_RDWR);
            close(m_fd);
            m_fd = -1;
        }
        if (m_thread.joinable()) m_thread.join();
    }

    bool is_running() const noexcept { return m_running.load(); }

private:
    // -----------------------------------------------------------------------
    // Connect to the ipcd Unix socket
    // -----------------------------------------------------------------------
    static int connect_to_ipcd(const std::string& path) {
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd == -1) return -1;

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        // Retry up to 3 seconds for ipcd to start
        for (int attempt = 0; attempt < 30; ++attempt) {
            if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
                return fd;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        close(fd);
        return -1;
    }

    // -----------------------------------------------------------------------
    // Write a complete protocol frame to ipcd
    // -----------------------------------------------------------------------
    bool send_frame(proto::MsgType type,
                    uint32_t       seq_id,
                    uint16_t       flags,
                    const uint8_t* payload,
                    uint32_t       payload_len) {
        proto::Header hdr{};
        hdr.magic       = proto::MAGIC;
        hdr.version     = proto::VER_1;
        hdr.msg_type    = static_cast<uint16_t>(type);
        hdr.flags       = flags;
        hdr.sequence_id = seq_id;
        hdr.payload_len = payload_len;
        hdr.checksum    = 0;

        if (write(m_fd, &hdr, sizeof(hdr)) != static_cast<ssize_t>(sizeof(hdr))) return false;
        if (payload_len > 0 && payload) {
            if (write(m_fd, payload, payload_len) != static_cast<ssize_t>(payload_len)) return false;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Send SYS_REGISTER_SERVICE("searchd") and wait for ACK
    // -----------------------------------------------------------------------
    bool register_service(const std::string& name) {
        const auto* data = reinterpret_cast<const uint8_t*>(name.data());
        if (!send_frame(proto::MsgType::SYS_REGISTER_SERVICE, 1, 0,
                        data, static_cast<uint32_t>(name.size()))) {
            return false;
        }
        // Read ACK (SYS_SERVICE_EVENT with 1-byte payload '1' for success)
        proto::Header ack_hdr{};
        ssize_t n = read(m_fd, &ack_hdr, sizeof(ack_hdr));
        if (n != static_cast<ssize_t>(sizeof(ack_hdr))) return false;
        if (ack_hdr.payload_len > 0) {
            std::vector<uint8_t> ack_payload(ack_hdr.payload_len);
            read(m_fd, ack_payload.data(), ack_hdr.payload_len);
            return !ack_payload.empty() && ack_payload[0] == 1;
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // run_loop — blocking read loop, dispatch SEARCH_QUERY messages
    // -----------------------------------------------------------------------
    void run_loop() {
        std::vector<uint8_t> buf;
        buf.reserve(8192);

        while (m_running) {
            // Read until we have a complete header
            uint8_t tmp[4096];
            ssize_t n = read(m_fd, tmp, sizeof(tmp));
            if (n <= 0) {
                if (!m_running) break;
                log::warn("[searchd-ipc] Connection to ipcd lost, stopping IPC loop");
                m_running = false;
                break;
            }
            buf.insert(buf.end(), tmp, tmp + n);

            // Drain all complete messages
            while (buf.size() >= sizeof(proto::Header)) {
                proto::Header hdr{};
                std::memcpy(&hdr, buf.data(), sizeof(hdr));

                if (hdr.magic != proto::MAGIC) {
                    log::error("[searchd-ipc] Bad magic 0x{:08X} — discarding connection", hdr.magic);
                    m_running = false;
                    break;
                }

                size_t total_needed = sizeof(hdr) + hdr.payload_len;
                if (buf.size() < total_needed) break; // wait for more data

                std::vector<uint8_t> payload(buf.begin() + static_cast<ptrdiff_t>(sizeof(hdr)),
                                             buf.begin() + static_cast<ptrdiff_t>(total_needed));
                buf.erase(buf.begin(), buf.begin() + static_cast<ptrdiff_t>(total_needed));

                handle_message(hdr, payload);
            }
        }

        log::info("[searchd-ipc] IPC loop exited");
    }

    // -----------------------------------------------------------------------
    // handle_message — dispatch a single decoded message
    // -----------------------------------------------------------------------
    void handle_message(const proto::Header& hdr, const std::vector<uint8_t>& payload) {
        auto type = static_cast<proto::MsgType>(hdr.msg_type);

        switch (type) {

        case proto::MsgType::SYS_PING:
            send_frame(proto::MsgType::SYS_PONG, hdr.sequence_id, 0, nullptr, 0);
            break;

        case proto::MsgType::SEARCH_QUERY: {
            // payload = raw UTF-8 query string
            std::string raw_query(reinterpret_cast<const char*>(payload.data()), payload.size());
            while (!raw_query.empty() && raw_query.back() == '\0') raw_query.pop_back();

            log::debug("[searchd-ipc] SEARCH_QUERY '{}' (seq={}, requester_fd={})",
                       raw_query, hdr.sequence_id, hdr.flags);

            auto t0 = std::chrono::steady_clock::now();

            // ------ Run the full ranking pipeline ---------------------------
            std::vector<uint8_t> result_payload;

            // Cache lookup
            auto cached = m_cache.get(raw_query, 0);
            if (cached.has_value()) {
                result_payload = WireSerializer::serialize_batch(*cached, 0);
            } else {
                // Normalize → provider fan-out → rank → top-N
                NormalizedQuery nq = QueryNormalizer::instance().normalize(raw_query);

                SearchSession sess(raw_query);
                auto candidates = m_mgr.execute_search(nq, 10, sess);
                sess.mark_stage("providers");

                m_ranker.rank(nq, candidates);
                auto& ranked = candidates;
                sess.mark_stage("rank");
                sess.complete();

                // Keep top 6 results
                if (ranked.size() > 6) ranked.resize(6);

                uint64_t latency_us = sess.total_latency_us();
                log::debug("[searchd-ipc] Query '{}' → {} results in {}µs",
                           raw_query, ranked.size(), latency_us);

                m_cache.put(raw_query, 0, ranked);
                result_payload = WireSerializer::serialize_batch(ranked, latency_us);
            }

            auto t1 = std::chrono::steady_clock::now();
            uint64_t elapsed_us = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());

            if (elapsed_us > 30'000) { // > 30ms: log perf warning
                log::warn("[searchd-ipc] ⚠  PERF: query '{}' took {}µs (target ≤30ms)",
                          raw_query, elapsed_us);
            }

            // Send SEARCH_RESULT reply back through ipcd.
            // ipcd uses hdr.flags to know which launcher fd to forward to.
            send_frame(proto::MsgType::SEARCH_RESULT,
                       hdr.sequence_id,
                       hdr.flags, // preserve requester_fd from ipcd forwarder
                       result_payload.empty() ? nullptr : result_payload.data(),
                       static_cast<uint32_t>(result_payload.size()));
            break;
        }

        default:
            log::debug("[searchd-ipc] Ignoring msg_type={}", hdr.msg_type);
            break;
        }
    }

    // -----------------------------------------------------------------------
    // Members
    // -----------------------------------------------------------------------
    ProviderManager& m_mgr;
    RankingPipeline& m_ranker;
    SearchCache&     m_cache;

    int             m_fd{-1};
    std::string     m_socket_path;
    std::atomic<bool> m_running{false};
    std::thread     m_thread;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCHD_IPC_SERVER_HPP
