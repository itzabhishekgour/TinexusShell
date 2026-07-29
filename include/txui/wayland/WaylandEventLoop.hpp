#pragma once

#include <txui/core/NonCopyable.hpp>
#include <txui/core/Types.hpp>

struct wl_display;

namespace txui::wayland {

class WaylandEventLoop final : public NonCopyable {
private:
    wl_display* m_display{nullptr};

public:
    explicit WaylandEventLoop(wl_display* display) noexcept;
    WaylandEventLoop() = default;
    WaylandEventLoop(WaylandEventLoop&& other) noexcept;
    WaylandEventLoop& operator=(WaylandEventLoop&& other) noexcept;
    ~WaylandEventLoop() noexcept = default;

    // Blocking wait for Wayland events from display socket
    void wait() noexcept;

    // Non-blocking poll for pending Wayland events
    void poll() noexcept;

    // Flush outgoing command buffer to Wayland display server
    void flush() noexcept;

    [[nodiscard]] bool is_valid() const noexcept { return m_display != nullptr; }
};

} // namespace txui::wayland
