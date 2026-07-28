#pragma once

#include <txui/core/NonCopyable.hpp>
#include <txui/core/Types.hpp>
#include <optional>
#include <string_view>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_shm;

namespace txui::wayland {

class WaylandConnection final : public NonCopyable {
private:
    wl_display* m_display{nullptr};
    wl_registry* m_registry{nullptr};
    wl_compositor* m_compositor{nullptr};
    wl_shm* m_shm{nullptr};

    explicit WaylandConnection(wl_display* display) noexcept;

public:
    WaylandConnection() = default;
    WaylandConnection(WaylandConnection&& other) noexcept;
    WaylandConnection& operator=(WaylandConnection&& other) noexcept;
    ~WaylandConnection() noexcept;

    // Connects to a Wayland server. Returns std::nullopt if the server cannot be reached.
    [[nodiscard]] static std::optional<WaylandConnection> connect(const char* display_name = nullptr) noexcept;

    // Performs a roundtrip to process all pending events from the compositor.
    void roundtrip() noexcept;

    // Flushes outgoing commands to the compositor socket.
    void flush() noexcept;

    [[nodiscard]] wl_display* display() const noexcept { return m_display; }
    [[nodiscard]] wl_compositor* compositor() const noexcept { return m_compositor; }
    [[nodiscard]] wl_shm* shm() const noexcept { return m_shm; }
    [[nodiscard]] bool is_valid() const noexcept {
        return m_display != nullptr && m_compositor != nullptr && m_shm != nullptr;
    }

    // Internal registry callback hooks
    void bind_compositor(wl_registry* registry, uint32 id, uint32 version) noexcept;
    void bind_shm(wl_registry* registry, uint32 id, uint32 version) noexcept;
};

} // namespace txui::wayland
