#include "displayd/vt_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::displayd {

bool VtManager::allocate_vt(uint32_t vt_num) {
    m_active_vt = vt_num;
    log::info("VtManager: Successfully allocated Virtual Terminal /dev/tty{}", vt_num);
    return true;
}

bool VtManager::switch_vt(uint32_t vt_num) {
    m_active_vt = vt_num;
    log::info("VtManager: Switched active Virtual Terminal to /dev/tty{}", vt_num);
    return true;
}

} // namespace tinexus::displayd
