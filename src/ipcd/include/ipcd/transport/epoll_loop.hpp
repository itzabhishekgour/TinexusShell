#ifndef TINEXUS_IPCD_TRANSPORT_EPOLL_LOOP_HPP
#define TINEXUS_IPCD_TRANSPORT_EPOLL_LOOP_HPP

#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include <functional>
#include "common/logger.hpp"

namespace tinexus::ipcd::transport {

class EpollLoop {
public:
    EpollLoop() {
        m_epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (m_epoll_fd == -1) {
            tinexus::log::error("Failed to create epoll instance");
        }
    }

    ~EpollLoop() {
        if (m_epoll_fd != -1) {
            close(m_epoll_fd);
        }
    }

    bool register_fd(int fd, uint32_t events, void* user_data) {
        struct epoll_event ev{};
        // Enforce Edge-Triggered (EPOLLET) for high performance zero-allocation loop
        ev.events = events | EPOLLET;
        ev.data.ptr = user_data;

        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
            tinexus::log::error("Failed to register fd {} with epoll", fd);
            return false;
        }
        return true;
    }

    bool unregister_fd(int fd) {
        if (epoll_ctl(m_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
            tinexus::log::error("Failed to unregister fd {} from epoll", fd);
            return false;
        }
        return true;
    }

    // Zero-allocation run loop. The event array is fixed size.
    void run(const std::function<void(const epoll_event&)>& callback) {
        constexpr int MAX_EVENTS = 64;
        struct epoll_event events[MAX_EVENTS];

        m_running = true;
        while (m_running) {
            int num_events = epoll_wait(m_epoll_fd, events, MAX_EVENTS, -1);
            if (num_events == -1) {
                if (errno == EINTR) continue;
                tinexus::log::error("epoll_wait failed");
                break;
            }

            for (int i = 0; i < num_events; ++i) {
                callback(events[i]);
            }
        }
    }

    void stop() {
        m_running = false;
    }

private:
    int m_epoll_fd{-1};
    bool m_running{false};
};

} // namespace tinexus::ipcd::transport

#endif // TINEXUS_IPCD_TRANSPORT_EPOLL_LOOP_HPP
