#ifndef TINEXUS_COMP_BACKEND_HPP
#define TINEXUS_COMP_BACKEND_HPP

#include <string>
#include <memory>
#include "comp/window/window_state.hpp"

struct wl_display;

namespace tinexus::comp {

enum class BackendType {
    Headless,
    Wlroots,
    X11
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void shutdown() = 0;
    virtual void close_active_window() noexcept = 0;
    virtual void snap_active_window(SnapMode mode) noexcept {}
    virtual void maximize_active_window() noexcept {}
    virtual void restore_active_window() noexcept {}
    virtual void fullscreen_active_window() noexcept {}
    virtual void focus_app(const std::string& app_id) noexcept {}
    virtual bool restore_window_by_app_id(const std::string& app_id) noexcept { return false; }
    virtual bool minimize_window_by_app_id(const std::string& app_id) noexcept { return false; }
    virtual bool has_app(const std::string& app_id) const noexcept { return false; }
    virtual bool toggle_launcher() noexcept { return false; }

    virtual struct wl_display* display() = 0;
    [[nodiscard]] virtual BackendType type() const noexcept = 0;

    // Session lock state — must be implemented by concrete backends
    virtual void set_locked(bool locked) noexcept = 0;
    [[nodiscard]] virtual bool is_locked() const noexcept = 0;

    // Test environment screendump & cursor automation
    virtual bool dump_screenshot(const std::string& path) { return false; }
    virtual void warp_cursor(double x, double y) {}
    virtual void simulate_click(uint32_t button, uint32_t state) {}
};


// Factory functions
std::unique_ptr<Backend> create_headless_backend();
std::unique_ptr<Backend> create_wlroots_backend(struct wl_display* display);

} // namespace tinexus::comp

#endif // TINEXUS_COMP_BACKEND_HPP
