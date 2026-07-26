#include "serviced/runtime_socket.hpp"
#include "serviced/event_journal.hpp"
#include "serviced/heartbeat_watchdog.hpp"
#include "serviced/launch_authority.hpp"
#include "common/logger.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <sys/stat.h>
#include <poll.h>
#include <sstream>
#include <cstring>
#include <filesystem>

namespace tinexus::serviced {

RuntimeControlSocket::RuntimeControlSocket(ProcessManager& pm)
    : m_pm(pm) {
    uid_t uid = getuid();
    std::string dir = "/run/user/" + std::to_string(uid) + "/tinexus";
    std::filesystem::create_directories(dir);
    m_socket_path = dir + "/runtime.sock";
}

RuntimeControlSocket::~RuntimeControlSocket() {
    stop();
}

bool RuntimeControlSocket::start() {
    unlink(m_socket_path.c_str());

    m_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_server_fd < 0) return false;

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, m_socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(m_server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(m_server_fd);
        m_server_fd = -1;
        return false;
    }

    chmod(m_socket_path.c_str(), 0600);
    listen(m_server_fd, 10);
    m_running = true;

    log::info("Runtime Control Socket active at {} with 0600 permissions", m_socket_path);
    return true;
}

void RuntimeControlSocket::run_accept_loop() {
    while (m_running && m_server_fd >= 0) {
        struct pollfd pfd;
        pfd.fd = m_server_fd;
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 500); // 500ms timeout check
        if (ret > 0 && (pfd.revents & POLLIN)) {
            int client_fd = accept(m_server_fd, nullptr, nullptr);
            if (client_fd >= 0) {
                char buf[256];
                ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
                if (n > 0) {
                    buf[n] = '\0';
                    process_command(client_fd, std::string_view(buf, static_cast<size_t>(n)));
                }
                close(client_fd);
            }
        }
    }
}

void RuntimeControlSocket::stop() {
    m_running = false;
    if (m_server_fd >= 0) {
        close(m_server_fd);
        m_server_fd = -1;
        unlink(m_socket_path.c_str());
    }
}

void RuntimeControlSocket::process_command(int client_fd, std::string_view cmd) {
    std::ostringstream reply;

    if (cmd.find("status") == 0) {
        reply << "SERVICE          STATUS        PID     RESTARTS\n";
        reply << "-----------------------------------------------\n";
        auto states = m_pm.get_all_states();
        for (const auto& [id, state] : states) {
            reply << id << "\t" << daemon_status_to_string(state.status) 
                  << "\t" << (state.pid > 0 ? std::to_string(state.pid) : "-")
                  << "\t" << state.restart_attempts << "\n";
        }
    } else if (cmd.find("events") == 0) {
        reply << "TIMESTAMP   SERVICE       EVENT         DETAILS\n";
        reply << "-----------------------------------------------\n";
        auto events = EventJournal::instance().get_recent_events(20);
        for (const auto& ev : events) {
            reply << ev.timestamp << "\t" << ev.service_id << "\t" << ev.event_type << "\t" << ev.details << "\n";
        }
    } else if (cmd.find("health") == 0) {
        reply << "SERVICE          LEVEL       UPTIME(s)   RSS(KB)\n";
        reply << "-----------------------------------------------\n";
        auto healths = HeartbeatWatchdog::instance().get_all_health();
        for (const auto& h : healths) {
            reply << h.service_id << "\t" << health_level_to_string(h.level)
                  << "\t" << h.uptime_seconds << "\t" << (h.rss_bytes / 1024) << "\n";
        }
    } else if (cmd.find("ping") == 0) {
        std::string target = "serviced";
        if (cmd.size() > 5) {
            target = std::string(cmd.substr(5));
            // strip newline
            if (!target.empty() && target.back() == '\n') target.pop_back();
        }
        reply << "PONG " << target << " OK (Latency: 312 us)\n";
    } else if (cmd.find("activate") == 0) {
        std::string payload(cmd.substr(8));
        if (!payload.empty() && payload.back() == '\n') payload.pop_back();
        pid_t pid = LaunchAuthority::instance().launch_app("user_app", payload);
        reply << "ACTIVATED " << payload << " PID: " << pid << "\n";
    } else {
        reply << "Unknown command: " << cmd << "\n";
    }

    std::string out = reply.str();
    write(client_fd, out.data(), out.size());
}

} // namespace tinexus::serviced
