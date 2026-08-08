#pragma once

#include <txui/core/NonCopyable.hpp>
#include <txui/core/Types.hpp>
#include <optional>
#include <string_view>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_shm;

struct xdg_wm_base;
struct wl_seat;
struct zwlr_layer_shell_v1;

namespace txui::wayland {

class WaylandConnection final : public NonCopyable {
private:
    wl_display* m_display{nullptr};
    wl_registry* m_registry{nullptr};
    wl_compositor* m_compositor{nullptr};
    wl_shm* m_shm{nullptr};
    xdg_wm_base* m_wm_base{nullptr};
    zwlr_layer_shell_v1* m_layer_shell{nullptr};
    wl_seat* m_seat{nullptr};

    explicit WaylandConnection(wl_display* display) noexcept;

public:
    WaylandConnection() = default;
    WaylandConnection(WaylandConnection&& other) noexcept;
    WaylandConnection& operator=(WaylandConnection&& other) noexcept;
    ~WaylandConnection() noexcept;

    // Establishes a connection to the Wayland display server.
    [[nodiscard]] static std::optional<WaylandConnection> connect(const char* display_name = nullptr) noexcept;

    // Dispatches pending events and blocks until at least one event is processed.
    void roundtrip() noexcept;

    // Flushes outgoing commands to the compositor socket.
    void flush() noexcept;

    [[nodiscard]] wl_display* display() const noexcept { return m_display; }
    [[nodiscard]] wl_compositor* compositor() const noexcept { return m_compositor; }
    [[nodiscard]] wl_shm* shm() const noexcept { return m_shm; }
    [[nodiscard]] xdg_wm_base* wm_base() const noexcept { return m_wm_base; }
    [[nodiscard]] zwlr_layer_shell_v1* layer_shell() const noexcept { return m_layer_shell; }
    [[nodiscard]] wl_seat* seat() const noexcept { return m_seat; }
    [[nodiscard]] bool is_valid() const noexcept {
        return m_display != nullptr && m_compositor != nullptr && m_shm != nullptr;
    }

    // Internal registry callback hooks
    void bind_compositor(wl_registry* registry, uint32 id, uint32 version) noexcept;
    void bind_shm(wl_registry* registry, uint32 id, uint32 version) noexcept;
    void bind_wm_base(wl_registry* registry, uint32_t name, uint32_t version) noexcept;
    void bind_layer_shell(wl_registry* registry, uint32_t name, uint32_t version) noexcept;
    void bind_seat(wl_registry* registry, uint32_t name, uint32_t version) noexcept;
};

} // namespace txui::wayland
