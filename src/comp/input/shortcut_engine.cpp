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

    constexpr uint32_t MOD_CTRL  = (1 << 2);
    constexpr uint32_t MOD_ALT   = (1 << 3);
    constexpr uint32_t MOD_LOGO  = (1 << 6);

    constexpr uint32_t KEY_K_EVDEV     = 37;
    constexpr uint32_t KEY_SPACE_EVDEV = 57;
    constexpr uint32_t KEY_LEFTMETA    = 125;
    constexpr uint32_t KEY_RIGHTMETA   = 126;

    bool has_ctrl = (modifiers & MOD_CTRL) != 0;
    bool has_alt  = (modifiers & MOD_ALT) != 0;
    bool is_k     = (keycode == KEY_K_EVDEV);
    bool is_space = (keycode == KEY_SPACE_EVDEV);
    bool is_super = (keycode == KEY_LEFTMETA || keycode == KEY_RIGHTMETA);

    // Trigger launcher on Ctrl+K, Ctrl+Space, Alt+Space, or Super key
    if ((has_ctrl && is_k) || (has_ctrl && is_space) || (has_alt && is_space) || is_super) {
        log::info("ShortcutEngine: Launcher shortcut triggered (keycode={}, mods={})! Dispatching activation.", keycode, modifiers);
        if (m_callback) {
            m_callback("launcher_toggle");
        }
        return true; // Intercepted by compositor
    }

    return false;
}

} // namespace tinexus::comp
