#include "comp/surface/surface_state.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

SurfaceState::SurfaceState(WindowID surface_id, const std::string& application_id)
    : id(surface_id), app_id(application_id) {
    log::info("SurfaceState: Allocated central surface state for WindowID #{} ('{}')", id, app_id);
}

void SurfaceState::commit_pending() {
    if (pending_buffer) {
        current_buffer = pending_buffer;
        pending_buffer = nullptr;
        mapped = true;
        log::info("SurfaceState: Committed pending buffer to current buffer for WindowID #{}", id);
    }
}

} // namespace tinexus::comp
