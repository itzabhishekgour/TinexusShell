#ifndef TINEXUS_SERVICED_IPCD_CLIENT_HPP
#define TINEXUS_SERVICED_IPCD_CLIENT_HPP

#include "serviced/install_handler.hpp"
#include "ipcd/protocol/header.hpp"
#include "ipcd/protocol/install.hpp"
#include "ipcd/protocol/uninstall.hpp"
#include "ipcd/transport/fd_passing.hpp"
#include "common/logger.hpp"
#include "common/RuntimePaths.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#include <thread>
#include <atomic>
#include <string>
#include <vector>
#include <cstring>

namespace tinexus::serviced {

class IpcdClient {
public:
    static IpcdClient& instance() noexcept {
        static IpcdClient client;
        return client;
    }

    bool start() {
        m_socket_path = tinexus::common::RuntimePaths::get_ipc_socket_path();

        m_running = true;
        m_thread = std::thread(&IpcdClient::run_loop, this);
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

private:
    IpcdClient() = default;
    ~IpcdClient() { stop(); }
    IpcdClient(const IpcdClient&) = delete;
    IpcdClient& operator=(const IpcdClient&) = delete;

    int connect_to_ipcd(const std::string& path) {
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd == -1) return -1;

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        for (int attempt = 0; attempt < 30; ++attempt) {
            if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0) {
                return fd;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        close(fd);
        return -1;
    }

    bool register_service(const std::string& name) {
        using namespace tinexus::ipcd::protocol;
        Header hdr{};
        hdr.magic = TINEXUS_IPC_MAGIC;
        hdr.version = TINEXUS_IPC_VERSION_1;
        hdr.msg_type = static_cast<uint16_t>(MessageType::SYS_REGISTER_SERVICE);
        hdr.payload_len = static_cast<uint32_t>(name.size() + 1); // include null
        
        std::vector<uint8_t> buf(sizeof(hdr) + hdr.payload_len);
        std::memcpy(buf.data(), &hdr, sizeof(hdr));
        std::memcpy(buf.data() + sizeof(hdr), name.c_str(), name.size() + 1);

        return write(m_fd, buf.data(), buf.size()) == static_cast<ssize_t>(buf.size());
    }

    void send_reply(uint32_t seq_id, tinexus::ipcd::protocol::MessageType type) {
        using namespace tinexus::ipcd::protocol;
        Header hdr{};
        hdr.magic = TINEXUS_IPC_MAGIC;
        hdr.version = TINEXUS_IPC_VERSION_1;
        hdr.msg_type = static_cast<uint16_t>(type);
        hdr.sequence_id = seq_id;
        hdr.payload_len = 0;
        
        write(m_fd, &hdr, sizeof(hdr));
    }

    void run_loop() {
        m_fd = connect_to_ipcd(m_socket_path);
        if (m_fd == -1) {
            log::warn("[serviced-ipc] ipcd not available at {} — running without IPC", m_socket_path);
            m_running = false;
            return;
        }

        if (!register_service("supervisor")) {
            log::error("[serviced-ipc] Failed to register with ipcd");
            close(m_fd);
            m_fd = -1;
            m_running = false;
            return;
        }

        log::info("[serviced-ipc] Registered as 'supervisor' with tinexus-ipcd");

        std::vector<uint8_t> read_buf;
        
        while (m_running) {
            struct pollfd pfd{};
            pfd.fd = m_fd;
            pfd.events = POLLIN;

            int poll_ret = ::poll(&pfd, 1, 500);
            if (poll_ret < 0) {
                if (errno == EINTR) continue;
                log::warn("[serviced-ipc] poll error on ipcd socket: {}", strerror(errno));
                break;
            }
            if (poll_ret == 0) {
                // Timeout (500ms), loops back to check m_running
                continue;
            }

            if (!(pfd.revents & POLLIN)) {
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                    log::warn("[serviced-ipc] Connection to ipcd hung up or error (revents=0x{:x})", pfd.revents);
                    break;
                }
                continue;
            }

            std::vector<int> pending_fds;
            ssize_t n = tinexus::ipcd::transport::recvmsg_with_fds(m_fd, read_buf, pending_fds);
            
            if (n <= 0) {
                if (n < 0 && (errno == EAGAIN || errno == EINTR)) continue;
                log::warn("[serviced-ipc] Connection to ipcd lost");
                break;
            }

            while (read_buf.size() >= sizeof(tinexus::ipcd::protocol::Header)) {
                tinexus::ipcd::protocol::Header hdr;
                std::memcpy(&hdr, read_buf.data(), sizeof(hdr));

                if (hdr.magic != tinexus::ipcd::protocol::TINEXUS_IPC_MAGIC) {
                    log::error("[serviced-ipc] Invalid IPC magic");
                    break;
                }

                if (read_buf.size() < sizeof(hdr) + hdr.payload_len) {
                    break; // Wait for full payload
                }

                if (hdr.msg_type == static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_INSTALL_REQUEST)) {
                    if (hdr.payload_len >= sizeof(tinexus::ipcd::protocol::InstallRequestPayload)) {
                        auto* req = reinterpret_cast<const tinexus::ipcd::protocol::InstallRequestPayload*>(read_buf.data() + sizeof(hdr));
                        std::vector<uint8_t> signature(req->signature.begin(), req->signature.end());
                        
                        std::string app_name(reinterpret_cast<const char*>(read_buf.data() + sizeof(hdr) + sizeof(tinexus::ipcd::protocol::InstallRequestPayload)), req->app_name_len);
                        
                        int payload_fd = -1;
                        if (!pending_fds.empty()) {
                            payload_fd = pending_fds.front();
                            pending_fds.erase(pending_fds.begin());
                        }

                        if (payload_fd != -1) {
                            bool success = InstallHandler::instance().handle_install_request(app_name, payload_fd, signature);
                            if (success) {
                                send_reply(hdr.sequence_id, tinexus::ipcd::protocol::MessageType::SYS_INSTALL_OK);
                            } else {
                                send_reply(hdr.sequence_id, tinexus::ipcd::protocol::MessageType::SYS_INSTALL_FAILED);
                            }
                            // InstallHandler takes ownership and closes payload_fd
                        } else {
                            log::error("[serviced-ipc] No FD received with INSTALL_REQUEST");
                            send_reply(hdr.sequence_id, tinexus::ipcd::protocol::MessageType::SYS_INSTALL_FAILED);
                        }
                    }
                } else if (hdr.msg_type == static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_UNINSTALL_REQUEST)) {
                    if (hdr.payload_len >= sizeof(tinexus::ipcd::protocol::UninstallRequestPayload)) {
                        auto* req = reinterpret_cast<const tinexus::ipcd::protocol::UninstallRequestPayload*>(read_buf.data() + sizeof(hdr));
                        // Ensure null termination safely
                        char safe_name[65] = {0};
                        std::strncpy(safe_name, req->app_name, 64);
                        std::string app_name(safe_name);
                        
                        bool success = InstallHandler::instance().handle_uninstall_request(app_name);
                        if (success) {
                            send_reply(hdr.sequence_id, tinexus::ipcd::protocol::MessageType::SYS_UNINSTALL_OK);
                        } else {
                            send_reply(hdr.sequence_id, tinexus::ipcd::protocol::MessageType::SYS_UNINSTALL_FAILED);
                        }
                    }
                }

                // Close any remaining FDs that weren't used
                for (int fd : pending_fds) {
                    close(fd);
                }
                pending_fds.clear();

                read_buf.erase(read_buf.begin(), read_buf.begin() + sizeof(hdr) + hdr.payload_len);
            }
        }
    }

    int m_fd{-1};
    std::string m_socket_path;
    std::atomic<bool> m_running{false};
    std::thread m_thread;
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_IPCD_CLIENT_HPP
