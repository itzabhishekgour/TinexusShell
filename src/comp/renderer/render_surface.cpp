#include "comp/renderer/render_surface.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

RenderSurface::RenderSurface(uint64_t surface_id) : id(surface_id) {
    log::info("RenderSurface: Allocated RenderSurface container for Surface #{}", id);
}

} // namespace tinexus::comp
