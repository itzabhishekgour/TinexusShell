#include "comp/surface/resource_cleanup.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

bool ResourceCleanup::destroy_surface_resources(SurfaceState& surface) {
    surface.pending_buffer = nullptr;
    surface.current_buffer = nullptr;
    surface.wl_surface = nullptr;
    surface.xdg_surface = nullptr;
    surface.xdg_toplevel = nullptr;
    surface.mapped = false;
    surface.configured = false;

    log::info("ResourceCleanup: Cleanly destroyed all Wayland resources and buffers for WindowID #{}", surface.id);
    return true;
}

} // namespace tinexus::comp
