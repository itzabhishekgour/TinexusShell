#include "comp/cursor/cursor_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

CursorManager& CursorManager::instance() noexcept {
    static CursorManager s_instance;
    return s_instance;
}

void CursorManager::set_theme(const std::string& theme_name, uint32_t size) {
    m_theme = theme_name;
    m_size = size;
    log::info("CursorManager: Theme set to '{}', size {}", m_theme, m_size);
}

void CursorManager::update_position(double x, double y) {
    m_pos.x = x;
    m_pos.y = y;
}

CursorPosition CursorManager::position() const noexcept {
    return m_pos;
}

std::string CursorManager::theme_name() const {
    return m_theme;
}

} // namespace tinexus::comp
