#pragma once

#include <txui/core/NonCopyable.hpp>
#include <txui/core/Types.hpp>
#include <functional>
#include <unordered_map>

struct wl_display;

namespace txui::wayland {

class WaylandEventLoop final : public NonCopyable {
public:
    using FdCallback = std::function<void(int fd, uint32_t revents)>;

private:
    wl_display* m_display{nullptr};
    std::unordered_map<int, FdCallback> m_extra_fds;

public:
    explicit WaylandEventLoop(wl_display* display) noexcept;
    WaylandEventLoop() = default;
    WaylandEventLoop(WaylandEventLoop&& other) noexcept;
    WaylandEventLoop& operator=(WaylandEventLoop&& other) noexcept;
    ~WaylandEventLoop() noexcept = default;

    // Blocking wait for Wayland events from display socket
    void wait() noexcept;

    // Blocking wait for Wayland events with timeout in milliseconds
    void wait_timeout(int timeout_ms) noexcept;

    // Non-blocking poll for pending Wayland events
    void poll() noexcept;

    // Flush outgoing command buffer to Wayland display server
    void flush() noexcept;

    // Register a custom file descriptor for polling
    void add_fd(int fd, FdCallback callback) noexcept;
    void remove_fd(int fd) noexcept;

    [[nodiscard]] bool is_valid() const noexcept { return m_display != nullptr; }
};

} // namespace txui::wayland
