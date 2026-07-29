// Force rebuild to resolve ODR violation
#include <txui/wayland/WaylandEventLoop.hpp>
#include <wayland-client.h>
#include <utility>

namespace txui::wayland {

WaylandEventLoop::WaylandEventLoop(wl_display* display) noexcept : m_display(display) {}

WaylandEventLoop::WaylandEventLoop(WaylandEventLoop&& other) noexcept
    : m_display(std::exchange(other.m_display, nullptr)) {}

WaylandEventLoop& WaylandEventLoop::operator=(WaylandEventLoop&& other) noexcept {
    if (this != &other) {
        m_display = std::exchange(other.m_display, nullptr);
    }
    return *this;
}

void WaylandEventLoop::wait() noexcept {
    if (m_display != nullptr) {
        wl_display_dispatch(m_display);
    }
}

void WaylandEventLoop::poll() noexcept {
    if (m_display != nullptr) {
        wl_display_dispatch_pending(m_display);
        wl_display_flush(m_display);
    }
}

void WaylandEventLoop::flush() noexcept {
    if (m_display != nullptr) {
        wl_display_flush(m_display);
    }
}

} // namespace txui::wayland
