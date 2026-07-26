#include "comp/focus/focus_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

FocusManager& FocusManager::instance() noexcept {
    static FocusManager s_instance;
    return s_instance;
}

void FocusManager::set_focus(FocusTargetType type, uint64_t surface_id, const std::string& target_id) {
    m_focus_type = type;
    m_surface_id = surface_id;
    m_target_id = target_id;

    log::info("FocusAuthority: Focus assigned to TargetType={} SurfaceID={} TargetID='{}'",
              focus_target_to_string(type), surface_id, target_id);
}

} // namespace tinexus::comp
