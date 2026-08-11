// Force rebuild to resolve ODR violation
#include <txui/wayland/WaylandEventLoop.hpp>
#include <wayland-client.h>
#include <poll.h>
#include <utility>
#include <vector>

namespace txui::wayland {

WaylandEventLoop::WaylandEventLoop(wl_display* display) noexcept : m_display(display) {}

WaylandEventLoop::WaylandEventLoop(WaylandEventLoop&& other) noexcept
    : m_display(std::exchange(other.m_display, nullptr)),
      m_extra_fds(std::move(other.m_extra_fds)) {}

WaylandEventLoop& WaylandEventLoop::operator=(WaylandEventLoop&& other) noexcept {
    if (this != &other) {
        m_display = std::exchange(other.m_display, nullptr);
        m_extra_fds = std::move(other.m_extra_fds);
    }
    return *this;
}

void WaylandEventLoop::wait() noexcept {
    if (m_display != nullptr) {
        wl_display_dispatch(m_display);
    }
}

void WaylandEventLoop::wait_timeout(int timeout_ms) noexcept {
    if (m_display != nullptr) {
        while (wl_display_prepare_read(m_display) != 0) {
            wl_display_dispatch_pending(m_display);
        }
        wl_display_flush(m_display);
        
        std::vector<struct pollfd> pfds;
        pfds.push_back({wl_display_get_fd(m_display), POLLIN, 0});
        for (const auto& [fd, _] : m_extra_fds) {
            pfds.push_back({fd, POLLIN, 0});
        }
        
        if (::poll(pfds.data(), pfds.size(), timeout_ms) > 0) {
            if (pfds[0].revents & POLLIN) {
                wl_display_read_events(m_display);
            } else {
                wl_display_cancel_read(m_display);
            }
            
            for (size_t i = 1; i < pfds.size(); ++i) {
                if (pfds[i].revents) {
                    if (auto it = m_extra_fds.find(pfds[i].fd); it != m_extra_fds.end()) {
                        it->second(pfds[i].fd, pfds[i].revents);
                    }
                }
            }
        } else {
            wl_display_cancel_read(m_display);
        }
        wl_display_dispatch_pending(m_display);
    }
}

void WaylandEventLoop::poll() noexcept {
    if (m_display != nullptr) {
        while (wl_display_prepare_read(m_display) != 0) {
            wl_display_dispatch_pending(m_display);
        }
        wl_display_flush(m_display);
        
        std::vector<struct pollfd> pfds;
        pfds.push_back({wl_display_get_fd(m_display), POLLIN, 0});
        for (const auto& [fd, _] : m_extra_fds) {
            pfds.push_back({fd, POLLIN, 0});
        }
        
        if (::poll(pfds.data(), pfds.size(), 0) > 0) {
            if (pfds[0].revents & POLLIN) {
                wl_display_read_events(m_display);
            } else {
                wl_display_cancel_read(m_display);
            }
            
            for (size_t i = 1; i < pfds.size(); ++i) {
                if (pfds[i].revents) {
                    if (auto it = m_extra_fds.find(pfds[i].fd); it != m_extra_fds.end()) {
                        it->second(pfds[i].fd, pfds[i].revents);
                    }
                }
            }
        } else {
            wl_display_cancel_read(m_display);
        }
        wl_display_dispatch_pending(m_display);
    }
}

void WaylandEventLoop::flush() noexcept {
    if (m_display != nullptr) {
        wl_display_flush(m_display);
    }
}

void WaylandEventLoop::add_fd(int fd, FdCallback callback) noexcept {
    m_extra_fds[fd] = std::move(callback);
}

void WaylandEventLoop::remove_fd(int fd) noexcept {
    m_extra_fds.erase(fd);
}

} // namespace txui::wayland
