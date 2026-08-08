#include <txui/wayland/WaylandConnection.hpp>
#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <cstring>
#include <utility>

namespace txui::wayland {

namespace {

void handle_global(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    auto* conn = static_cast<WaylandConnection*>(data);
    if (std::strcmp(interface, "wl_compositor") == 0) {
        conn->bind_compositor(registry, name, version);
    } else if (std::strcmp(interface, "wl_shm") == 0) {
        conn->bind_shm(registry, name, version);
    } else if (std::strcmp(interface, "xdg_wm_base") == 0) {
        conn->bind_wm_base(registry, name, version);
    } else if (std::strcmp(interface, "zwlr_layer_shell_v1") == 0) {
        conn->bind_layer_shell(registry, name, version);
    } else if (std::strcmp(interface, "wl_seat") == 0) {
        conn->bind_seat(registry, name, version);
    }
}

void handle_global_remove(void* /*data*/, struct wl_registry* /*registry*/, uint32_t /*name*/) {
    // No-op for global removal in basic connection lifecycle
}

const struct wl_registry_listener registry_listener = {
    .global = handle_global,
    .global_remove = handle_global_remove
};

} // namespace

WaylandConnection::WaylandConnection(wl_display* display) noexcept : m_display(display) {
    if (m_display != nullptr) {
        m_registry = wl_display_get_registry(m_display);
        if (m_registry != nullptr) {
            wl_registry_add_listener(m_registry, &registry_listener, this);
            wl_display_roundtrip(m_display);
        }
    }
}

WaylandConnection::WaylandConnection(WaylandConnection&& other) noexcept
    : m_display(std::exchange(other.m_display, nullptr)),
      m_registry(std::exchange(other.m_registry, nullptr)),
      m_compositor(std::exchange(other.m_compositor, nullptr)),
      m_shm(std::exchange(other.m_shm, nullptr)),
      m_wm_base(std::exchange(other.m_wm_base, nullptr)),
      m_layer_shell(std::exchange(other.m_layer_shell, nullptr)),
      m_seat(std::exchange(other.m_seat, nullptr)) {}

WaylandConnection& WaylandConnection::operator=(WaylandConnection&& other) noexcept {
    if (this != &other) {
        if (m_wm_base != nullptr) xdg_wm_base_destroy(m_wm_base);
        if (m_layer_shell != nullptr) zwlr_layer_shell_v1_destroy(m_layer_shell);
        if (m_seat != nullptr) wl_seat_destroy(m_seat);
        if (m_compositor != nullptr) wl_compositor_destroy(m_compositor);
        if (m_shm != nullptr) wl_shm_destroy(m_shm);
        if (m_registry != nullptr) wl_registry_destroy(m_registry);
        if (m_display != nullptr) wl_display_disconnect(m_display);

        m_display = std::exchange(other.m_display, nullptr);
        m_registry = std::exchange(other.m_registry, nullptr);
        m_compositor = std::exchange(other.m_compositor, nullptr);
        m_shm = std::exchange(other.m_shm, nullptr);
        m_wm_base = std::exchange(other.m_wm_base, nullptr);
        m_layer_shell = std::exchange(other.m_layer_shell, nullptr);
        m_seat = std::exchange(other.m_seat, nullptr);
    }
    return *this;
}

WaylandConnection::~WaylandConnection() noexcept {
    if (m_wm_base != nullptr) xdg_wm_base_destroy(m_wm_base);
    if (m_layer_shell != nullptr) zwlr_layer_shell_v1_destroy(m_layer_shell);
    if (m_seat != nullptr) wl_seat_destroy(m_seat);
    if (m_compositor != nullptr) wl_compositor_destroy(m_compositor);
    if (m_shm != nullptr) wl_shm_destroy(m_shm);
    if (m_registry != nullptr) wl_registry_destroy(m_registry);
    if (m_display != nullptr) wl_display_disconnect(m_display);
}

std::optional<WaylandConnection> WaylandConnection::connect(const char* display_name) noexcept {
    wl_display* display = wl_display_connect(display_name);
    if (display == nullptr) {
        return std::nullopt;
    }
    WaylandConnection conn(display);
    if (!conn.is_valid()) {
        return std::nullopt;
    }
    return conn;
}

void WaylandConnection::roundtrip() noexcept {
    if (m_display != nullptr) {
        wl_display_roundtrip(m_display);
    }
}

void WaylandConnection::flush() noexcept {
    if (m_display != nullptr) {
        wl_display_flush(m_display);
    }
}

void WaylandConnection::bind_compositor(wl_registry* registry, uint32_t name, uint32_t version) noexcept {
    uint32_t bind_ver = (version < 4U) ? version : 4U;
    m_compositor = static_cast<wl_compositor*>(
        wl_registry_bind(registry, name, &wl_compositor_interface, bind_ver)
    );
}

void WaylandConnection::bind_shm(wl_registry* registry, uint32_t name, uint32_t version) noexcept {
    uint32_t bind_ver = (version < 1U) ? version : 1U;
    m_shm = static_cast<wl_shm*>(
        wl_registry_bind(registry, name, &wl_shm_interface, bind_ver)
    );
}

void WaylandConnection::bind_wm_base(wl_registry* registry, uint32_t name, uint32_t version) noexcept {
    uint32_t bind_ver = (version < 3U) ? version : 3U;
    m_wm_base = static_cast<xdg_wm_base*>(
        wl_registry_bind(registry, name, &xdg_wm_base_interface, bind_ver)
    );
}

void WaylandConnection::bind_layer_shell(wl_registry* registry, uint32_t name, uint32_t version) noexcept {
    m_layer_shell = static_cast<zwlr_layer_shell_v1*>(
        wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version >= 4 ? 4 : version)
    );
}

void WaylandConnection::bind_seat(wl_registry* registry, uint32_t name, uint32_t version) noexcept {
    uint32_t bind_ver = (version < 7U) ? version : 7U;
    m_seat = static_cast<wl_seat*>(
        wl_registry_bind(registry, name, &wl_seat_interface, bind_ver)
    );
}

} // namespace txui::wayland
