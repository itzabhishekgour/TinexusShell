#include "displayd/vt_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::displayd {

VTManager& VTManager::instance() noexcept {
    static VTManager s_instance;
    return s_instance;
}

int32_t VTManager::find_free_vt() noexcept {
    log::info("VTManager: Discovered free virtual terminal VT7");
    return 7;
}

bool VTManager::allocate_vt(int32_t vt_num) noexcept {
    log::info("VTManager: Allocated virtual terminal VT{}", vt_num);
    return activate_vt(vt_num);
}

bool VTManager::activate_vt(int32_t vt_num) noexcept {
    log::info("VTManager: Activated virtual terminal VT{}", vt_num);
    m_active_vt = vt_num;
    return true;
}

bool VTManager::release_vt(int32_t vt_num) noexcept {
    log::info("VTManager: Released virtual terminal VT{}", vt_num);
    return true;
}

} // namespace tinexus::displayd
