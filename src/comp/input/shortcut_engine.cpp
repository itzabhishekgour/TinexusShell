#include "comp/input/shortcut_engine.hpp"
#include "common/logger.hpp"
#include <string>

namespace tinexus::comp {

ShortcutEngine& ShortcutEngine::instance() noexcept {
    static ShortcutEngine s_instance;
    return s_instance;
}

void ShortcutEngine::set_shortcut_callback(ShortcutCallback cb) {
    m_callback = std::move(cb);
}

bool ShortcutEngine::process_key_event(uint32_t modifiers, uint32_t keycode, bool is_pressed, uint32_t keysym) {
    if (!is_pressed) return false;

    constexpr uint32_t MOD_CTRL  = (1 << 2);
    constexpr uint32_t MOD_ALT   = (1 << 3);
    constexpr uint32_t MOD_LOGO  = (1 << 6); // Super/Win key as modifier

    // Evdev keycodes
    constexpr uint32_t KEY_K          = 37;
    constexpr uint32_t KEY_SPACE      = 57;
    constexpr uint32_t KEY_LEFTMETA   = 125;
    constexpr uint32_t KEY_RIGHTMETA  = 126;
    constexpr uint32_t KEY_L          = 38;   // Super+L  → lock
    constexpr uint32_t KEY_TAB        = 15;   // Alt+Tab  → window switcher
    constexpr uint32_t KEY_LEFT       = 105;  // Super+Left  → snap left
    constexpr uint32_t KEY_RIGHT      = 106;  // Super+Right → snap right
    constexpr uint32_t KEY_UP         = 103;  // Super+Up    → maximize
    constexpr uint32_t KEY_DOWN       = 108;  // Super+Down  → restore
    constexpr uint32_t KEY_Q          = 16;   // Super+Q     → close window
    // Number keys 1–9 (evdev codes 2–10)
    constexpr uint32_t KEY_1          = 2;
    constexpr uint32_t KEY_9          = 10;
    // Multimedia keys (evdev codes)
    constexpr uint32_t KEY_MUTE            = 113;
    constexpr uint32_t KEY_VOLUMEDOWN       = 114;
    constexpr uint32_t KEY_VOLUMEUP         = 115;
    constexpr uint32_t KEY_BRIGHTNESSDOWN   = 232;
    constexpr uint32_t KEY_BRIGHTNESSUP     = 233;

    // Keysyms (XKB)
    constexpr uint32_t SYM_k          = 0x006b;
    constexpr uint32_t SYM_K          = 0x004b;
    constexpr uint32_t SYM_space      = 0x0020;
    constexpr uint32_t SYM_AudioLowerVolume = 0x1008ff11;
    constexpr uint32_t SYM_AudioMute        = 0x1008ff12;
    constexpr uint32_t SYM_AudioRaiseVolume = 0x1008ff13;
    constexpr uint32_t SYM_MonBrightnessDown= 0x1008ff03;
    constexpr uint32_t SYM_MonBrightnessUp  = 0x1008ff02;

    bool has_ctrl  = (modifiers & MOD_CTRL) != 0;
    bool has_alt   = (modifiers & MOD_ALT)  != 0;
    bool has_super = (modifiers & MOD_LOGO) != 0;
    bool is_super_key = (keycode == KEY_LEFTMETA || keycode == KEY_RIGHTMETA);

    if (!m_callback) return false;

    // ── Multimedia: Volume & Brightness Keys ─────────────────────────────────
    if (keycode == KEY_VOLUMEUP || keysym == SYM_AudioRaiseVolume) {
        log::info("ShortcutEngine: volume_up (keycode={}, keysym=0x{:x})", keycode, keysym);
        m_callback("volume_up");
        return true;
    }
    if (keycode == KEY_VOLUMEDOWN || keysym == SYM_AudioLowerVolume) {
        log::info("ShortcutEngine: volume_down (keycode={}, keysym=0x{:x})", keycode, keysym);
        m_callback("volume_down");
        return true;
    }
    if (keycode == KEY_MUTE || keysym == SYM_AudioMute) {
        log::info("ShortcutEngine: volume_mute (keycode={}, keysym=0x{:x})", keycode, keysym);
        m_callback("volume_mute");
        return true;
    }
    if (keycode == KEY_BRIGHTNESSUP || keysym == SYM_MonBrightnessUp) {
        log::info("ShortcutEngine: brightness_up (keycode={}, keysym=0x{:x})", keycode, keysym);
        m_callback("brightness_up");
        return true;
    }
    if (keycode == KEY_BRIGHTNESSDOWN || keysym == SYM_MonBrightnessDown) {
        log::info("ShortcutEngine: brightness_down (keycode={}, keysym=0x{:x})", keycode, keysym);
        m_callback("brightness_down");
        return true;
    }

    // ── Launcher: Ctrl+K, Ctrl+Space, Alt+Space ─────────────────────────────
    bool is_k = (keycode == KEY_K || keysym == SYM_k || keysym == SYM_K);
    bool is_space = (keycode == KEY_SPACE || keysym == SYM_space);

    if ((has_ctrl && is_k) ||
        (has_ctrl && is_space) ||
        (has_alt  && is_space)) {
        log::info("ShortcutEngine: launcher_toggle (keycode={}, keysym=0x{:x}, mods={})", keycode, keysym, modifiers);
        m_callback("launcher_toggle");
        return true;
    }

    // ── Lock screen: Super+L ─────────────────────────────────────────────────
    if (has_super && keycode == KEY_L) {
        log::info("ShortcutEngine: lock_screen");
        m_callback("lock_screen");
        return true;
    }

    // ── Window snapping: Super+Left / Right / Up / Down ──────────────────────
    if (has_super && keycode == KEY_LEFT) {
        log::info("ShortcutEngine: snap_left");
        m_callback("snap_left");
        return true;
    }
    if (has_super && keycode == KEY_RIGHT) {
        log::info("ShortcutEngine: snap_right");
        m_callback("snap_right");
        return true;
    }
    if (has_super && keycode == KEY_UP) {
        log::info("ShortcutEngine: maximize");
        m_callback("maximize");
        return true;
    }
    if (has_super && keycode == KEY_DOWN) {
        log::info("ShortcutEngine: restore");
        m_callback("restore");
        return true;
    }

    // ── Close window: Super+Q ────────────────────────────────────────────────
    if (has_super && keycode == KEY_Q) {
        log::info("ShortcutEngine: close_window");
        m_callback("close_window");
        return true;
    }

    // ── Alt+Tab window switcher ───────────────────────────────────────────────
    if (has_alt && keycode == KEY_TAB) {
        bool reverse = (modifiers & MOD_CTRL) != 0; // Alt+Ctrl+Tab = reverse
        log::info("ShortcutEngine: alttab_switch (reverse={})", reverse);
        m_callback(reverse ? "alttab_prev" : "alttab_next");
        return true;
    }

    // ── Super+Tab workspace overview (Mission Control) ───────────────────────
    if (has_super && keycode == KEY_TAB) {
        log::info("ShortcutEngine: workspace_overview_toggle");
        m_callback("workspace_overview_toggle");
        return true;
    }

    // ── Workspace switching: Super+1–9 ────────────────────────────────────────
    if (has_super && keycode >= KEY_1 && keycode <= KEY_9) {
        uint32_t ws_num = keycode - KEY_1 + 1; // 1–9
        std::string action = "workspace_switch_" + std::to_string(ws_num);
        log::info("ShortcutEngine: {} (keycode={})", action, keycode);
        m_callback(action);
        return true;
    }

    return false;
}

} // namespace tinexus::comp
