#ifndef TINEXUS_IPCD_TRANSPORT_UNIX_SOCKET_HPP
#define TINEXUS_IPCD_TRANSPORT_UNIX_SOCKET_HPP

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <system_error>
#include "common/logger.hpp"

namespace tinexus::ipcd::transport {

class UnixSocketServer {
public:
    UnixSocketServer(const std::string& path) : m_path(path), m_fd(-1) {}
    
    ~UnixSocketServer() {
        if (m_fd != -1) {
            close(m_fd);
            unlink(m_path.c_str());
        }
    }

    bool bind_and_listen() {
        // Ensure parent directory exists with 0700 permissions
        std::string dir_path = m_path.substr(0, m_path.find_last_of('/'));
        mkdir(dir_path.c_str(), 0700);
        chmod(dir_path.c_str(), 0700); // Enforce permissions

        m_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (m_fd == -1) {
            tinexus::log::error("Failed to create Unix socket");
            return false;
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, m_path.c_str(), sizeof(addr.sun_path) - 1);

        unlink(m_path.c_str());

        if (bind(m_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            tinexus::log::error("Failed to bind Unix socket at {}", m_path);
            close(m_fd);
            m_fd = -1;
            return false;
        }

        // Enforce 0600 permissions on the socket file itself
        if (chmod(m_path.c_str(), 0600) == -1) {
            tinexus::log::error("Failed to set 0600 permissions on {}", m_path);
            close(m_fd);
            m_fd = -1;
            return false;
        }

        if (listen(m_fd, 128) == -1) {
            tinexus::log::error("Failed to listen on Unix socket");
            close(m_fd);
            m_fd = -1;
            return false;
        }

        tinexus::log::info("Unix socket listening at {} with 0600 permissions", m_path);
        return true;
    }

    int get_fd() const { return m_fd; }

private:
    std::string m_path;
    int m_fd;
};

} // namespace tinexus::ipcd::transport

#endif // TINEXUS_IPCD_TRANSPORT_UNIX_SOCKET_HPP
