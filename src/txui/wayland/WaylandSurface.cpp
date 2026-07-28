#include <txui/wayland/WaylandSurface.hpp>
#include <wayland-client.h>
#include <utility>

namespace txui::wayland {

WaylandSurface::WaylandSurface(wl_surface* surface) noexcept : m_surface(surface) {}

WaylandSurface::WaylandSurface(WaylandSurface&& other) noexcept
    : m_surface(std::exchange(other.m_surface, nullptr)) {}

WaylandSurface& WaylandSurface::operator=(WaylandSurface&& other) noexcept {
    if (this != &other) {
        if (m_surface != nullptr) {
            wl_surface_destroy(m_surface);
        }
        m_surface = std::exchange(other.m_surface, nullptr);
    }
    return *this;
}

WaylandSurface::~WaylandSurface() noexcept {
    if (m_surface != nullptr) {
        wl_surface_destroy(m_surface);
    }
}

WaylandSurface WaylandSurface::create(wl_compositor* compositor) noexcept {
    if (compositor == nullptr) {
        return WaylandSurface();
    }
    return WaylandSurface(wl_compositor_create_surface(compositor));
}

void WaylandSurface::attach(WaylandBuffer& buffer, int32 x, int32 y) noexcept {
    if (m_surface != nullptr && buffer.buffer() != nullptr) {
        wl_surface_attach(m_surface, buffer.buffer(), x, y);
    }
}

void WaylandSurface::damage_buffer(int32 x, int32 y, int32 width, int32 height) noexcept {
    if (m_surface != nullptr) {
        wl_surface_damage_buffer(m_surface, x, y, width, height);
    }
}

void WaylandSurface::commit() noexcept {
    if (m_surface != nullptr) {
        wl_surface_commit(m_surface);
    }
}

} // namespace txui::wayland
