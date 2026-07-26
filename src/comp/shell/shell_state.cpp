#include "comp/shell/shell_state.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

ShellState& ShellState::instance() noexcept {
    static ShellState s_instance;
    return s_instance;
}

bool ShellState::launcher_visible() const noexcept {
    return m_launcher_visible;
}

void ShellState::set_launcher_visible(bool visible) {
    if (m_launcher_visible != visible) {
        m_launcher_visible = visible;
        log::info("ShellState: Launcher visible -> {}", visible);
    }
}

bool ShellState::lockscreen_active() const noexcept {
    return m_lockscreen_active;
}

void ShellState::set_lockscreen_active(bool active) {
    if (m_lockscreen_active != active) {
        m_lockscreen_active = active;
        log::info("ShellState: Lockscreen active -> {}", active);
    }
}

bool ShellState::overview_active() const noexcept {
    return m_overview_active;
}

void ShellState::set_overview_active(bool active) {
    if (m_overview_active != active) {
        m_overview_active = active;
        log::info("ShellState: Overview active -> {}", active);
    }
}

} // namespace tinexus::comp
