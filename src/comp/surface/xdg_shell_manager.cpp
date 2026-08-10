#include "comp/surface/xdg_shell_manager.hpp"
#include "common/logger.hpp"
#include "comp/window/window_manager.hpp"

namespace tinexus::comp {

bool XdgShellManager::bind_xdg_wm_base(uint32_t version) {
    log::info("XdgShellManager: Bound xdg_wm_base protocol interface (version {})", version);
    return true;
}

uint64_t XdgShellManager::create_xdg_surface(uint64_t surface_id) {
    SurfaceState surface(surface_id, "wayland-client");
    m_xdg_surfaces[surface_id] = surface;
    log::info("XdgShellManager: Created xdg_surface for Surface #{}", surface_id);
    return surface_id;
}

uint32_t XdgShellManager::send_toplevel_configure(uint64_t surface_id, int32_t width, int32_t height) {
    auto it = m_xdg_surfaces.find(surface_id);
    if (it == m_xdg_surfaces.end()) return 0;

    uint32_t serial = m_serial_mgr.generate();
    it->second.last_configure_serial = serial;
    it->second.geometry = {0, 0, static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    log::info("XdgShellManager: Sent toplevel configure serial #{} ({}x{}) to Surface #{}", serial, width, height, surface_id);
    return serial;
}

bool XdgShellManager::handle_ack_configure(uint64_t surface_id, uint32_t serial) {
    auto it = m_xdg_surfaces.find(surface_id);
    if (it == m_xdg_surfaces.end()) return false;

    if (m_serial_mgr.validate(serial)) {
        it->second.configured = true;
        
        auto win = WindowManager::instance().find_window(surface_id);
        if (win) {
            win->saved_w = it->second.geometry.width;
            win->saved_h = it->second.geometry.height;
        }
        
        log::info("XdgShellManager: Validated client ack_configure serial #{} for Surface #{}", serial, surface_id);
        return true;
    }
    return false;
}

} // namespace tinexus::comp
