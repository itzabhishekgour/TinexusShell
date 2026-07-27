#ifndef TINEXUS_COMP_SERVER_HPP
#define TINEXUS_COMP_SERVER_HPP

#include "comp/server/global_registry.hpp"
#include "comp/server/surface_tree.hpp"
#include <string>

struct wl_display;
struct wl_event_loop;

namespace tinexus::comp {

class TinexusServer {
public:
    TinexusServer();
    ~TinexusServer();

    bool initialize();
    void run();
    void stop();

    [[nodiscard]] const std::string& wayland_display() const noexcept;
    [[nodiscard]] struct wl_display* display_handle() const noexcept { return m_wl_display; }
    [[nodiscard]] GlobalRegistry& registry() noexcept { return m_registry; }
    [[nodiscard]] SurfaceTree& surface_tree() noexcept { return m_surface_tree; }

private:
    struct wl_display* m_wl_display{nullptr};
    struct wl_event_loop* m_wl_loop{nullptr};
    std::string m_display_socket{"wayland-0"};
    bool m_running{false};
    GlobalRegistry m_registry;
    SurfaceTree m_surface_tree;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SERVER_HPP
