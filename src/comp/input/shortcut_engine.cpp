#include "comp/input/shortcut_engine.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

ShortcutEngine& ShortcutEngine::instance() noexcept {
    static ShortcutEngine s_instance;
    return s_instance;
}

void ShortcutEngine::set_shortcut_callback(ShortcutCallback cb) {
    m_callback = std::move(cb);
}

bool ShortcutEngine::process_key_event(uint32_t modifiers, uint32_t keycode, bool is_pressed) {
    if (!is_pressed) return false;

    constexpr uint32_t MOD_CTRL = (1 << 2);
    constexpr uint32_t KEY_K = 37; // Linux evdev KEY_K / XKB keycode for K

    // Check Ctrl + K shortcut
    if ((modifiers & MOD_CTRL) && (keycode == KEY_K)) {
        log::info("ShortcutEngine: Ctrl+K triggered! Dispatching shortcut activation event.");
        if (m_callback) {
            m_callback("launcher_toggle");
        }
        return true; // Intercepted by compositor
    }

    return false;
}

} // namespace tinexus::comp
