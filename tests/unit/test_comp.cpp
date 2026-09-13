#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/workspace/workspace_manager.hpp"
#include "comp/window/window_manager.hpp"
#include "comp/focus/focus_manager.hpp"
#include "comp/shell/shell_state.hpp"
#include "comp/render/frame_scheduler.hpp"
#include "comp/cursor/cursor_manager.hpp"
#include "comp/output/output_manager.hpp"
#include "comp/input/shortcut_engine.hpp"

void test_workspace_manager() {
    auto& mgr = tinexus::comp::WorkspaceManager::instance();
    mgr.initialize_default_workspaces(3);

    assert(mgr.active_workspace_id() == 1);
    assert(mgr.get_all_workspaces().size() == 3);

    assert(mgr.switch_workspace(2));
    assert(mgr.active_workspace_id() == 2);

    mgr.add_window_to_workspace(2, 101);
    assert(mgr.get_workspace(2)->window_ids.size() == 1);

    mgr.remove_window_from_workspace(101);
    assert(mgr.get_workspace(2)->window_ids.empty());

    std::cout << "[PASS] test_workspace_manager\n";
}

void test_window_focus_manager() {
    auto& win_mgr = tinexus::comp::WindowManager::instance();
    auto& focus_mgr = tinexus::comp::FocusManager::instance();

    uint64_t w1 = win_mgr.register_window(1234, "org.mozilla.firefox", "Firefox Web Browser");
    assert(w1 > 0);
    assert(focus_mgr.current_surface_id() == w1);
    assert(focus_mgr.current_target_id() == "org.mozilla.firefox");

    win_mgr.set_geometry(w1, 0, 0, 1024, 768);
    auto info = win_mgr.get_window(w1);
    assert(info.has_value());
    assert(info->width == 1024);
    assert(info->height == 768);

    win_mgr.unregister_window(w1);
    assert(!win_mgr.get_window(w1).has_value());

    std::cout << "[PASS] test_window_focus_manager\n";
}

void test_shell_state() {
    auto& state = tinexus::comp::ShellState::instance();
    assert(!state.launcher_visible());

    state.set_launcher_visible(true);
    assert(state.launcher_visible());

    state.set_lockscreen_active(true);
    assert(state.lockscreen_active());

    state.set_launcher_visible(false);
    assert(!state.launcher_visible());

    std::cout << "[PASS] test_shell_state\n";
}

void test_frame_scheduler() {
    auto& sched = tinexus::comp::FrameScheduler::instance();
    sched.set_target_refresh_rate(144);
    assert(sched.target_refresh_rate() == 144);

    sched.notify_damage();
    sched.on_vblank();

    auto stats = sched.stats();
    assert(stats.frame_count >= 1);
    assert(!stats.damage_pending);

    std::cout << "[PASS] test_frame_scheduler\n";
}

void test_shortcut_engine() {
    auto& engine = tinexus::comp::ShortcutEngine::instance();
    std::string last_shortcut;

    engine.set_shortcut_callback([&last_shortcut](const std::string& shortcut) {
        last_shortcut = shortcut;
    });

    constexpr uint32_t MOD_NONE  = 0;
    constexpr uint32_t MOD_CTRL  = (1 << 2);
    constexpr uint32_t MOD_ALT   = (1 << 3);
    constexpr uint32_t MOD_LOGO  = (1 << 6);

    constexpr uint32_t KEY_K          = 37;
    constexpr uint32_t KEY_A          = 30;
    constexpr uint32_t KEY_SPACE      = 57;
    constexpr uint32_t KEY_LEFTMETA   = 125;
    constexpr uint32_t KEY_RIGHTMETA  = 126;
    constexpr uint32_t KEY_L          = 38;
    constexpr uint32_t KEY_TAB        = 15;
    constexpr uint32_t KEY_LEFT       = 105;
    constexpr uint32_t KEY_RIGHT      = 106;
    constexpr uint32_t KEY_UP         = 103;
    constexpr uint32_t KEY_DOWN       = 108;
    constexpr uint32_t KEY_VOLUMEUP   = 115;
    constexpr uint32_t KEY_VOLUMEDOWN = 114;
    constexpr uint32_t KEY_MUTE       = 113;

    // 1. Phase G: Ctrl+K opens launcher and returns true (swallowed)
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_CTRL, KEY_K, true, 0x006b) == true);
    assert(last_shortcut == "launcher_toggle");

    // 2. Phase G: Key release (is_pressed=false) must NEVER be swallowed
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_CTRL, KEY_K, false, 0x006b) == false);
    assert(last_shortcut.empty());

    // 3. Phase G: Ordinary key press (e.g. typing 'k' or 'a' without Ctrl) must NEVER be swallowed
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_NONE, KEY_K, true, 0x006b) == false);
    assert(last_shortcut.empty());
    assert(engine.process_key_event(MOD_NONE, KEY_A, true, 0x0061) == false);
    assert(last_shortcut.empty());

    // 4. Phase G: Alternative launcher triggers
    // Ctrl+Space
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_CTRL, KEY_SPACE, true, 0x0020) == true);
    assert(last_shortcut == "launcher_toggle");

    // Bare Super/Meta key
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_NONE, KEY_LEFTMETA, true, 0) == true);
    assert(last_shortcut == "launcher_toggle");
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_NONE, KEY_RIGHTMETA, true, 0) == true);
    assert(last_shortcut == "launcher_toggle");

    // 5. Phase G: Lock screen shortcut (Super+L)
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_LOGO, KEY_L, true, 0x006c) == true);
    assert(last_shortcut == "lock_screen");

    // 6. Phase G: Window snapping shortcuts (Super+Arrows)
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_LOGO, KEY_LEFT, true, 0) == true);
    assert(last_shortcut == "snap_left");

    last_shortcut.clear();
    assert(engine.process_key_event(MOD_LOGO, KEY_RIGHT, true, 0) == true);
    assert(last_shortcut == "snap_right");

    last_shortcut.clear();
    assert(engine.process_key_event(MOD_LOGO, KEY_UP, true, 0) == true);
    assert(last_shortcut == "maximize");

    last_shortcut.clear();
    assert(engine.process_key_event(MOD_LOGO, KEY_DOWN, true, 0) == true);
    assert(last_shortcut == "restore");

    // 7. Phase G: Window switcher (Alt+Tab)
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_ALT, KEY_TAB, true, 0) == true);
    assert(last_shortcut == "alttab_next");

    // 8. Phase G: Multimedia audio keys
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_NONE, KEY_VOLUMEUP, true, 0x1008ff13) == true);
    assert(last_shortcut == "volume_up");

    last_shortcut.clear();
    assert(engine.process_key_event(MOD_NONE, KEY_VOLUMEDOWN, true, 0x1008ff11) == true);
    assert(last_shortcut == "volume_down");

    last_shortcut.clear();
    assert(engine.process_key_event(MOD_NONE, KEY_MUTE, true, 0x1008ff12) == true);
    assert(last_shortcut == "volume_mute");

    std::cout << "[PASS] test_shortcut_engine (Phase G: Ctrl+K & Input Swallowing Verified)\n";
}

int main() {
    tinexus::log::set_component_name("unit_test_comp");
    tinexus::log::info("Running unit tests for Compositor...");

    test_workspace_manager();
    test_window_focus_manager();
    test_shell_state();
    test_frame_scheduler();
    test_shortcut_engine();

    tinexus::log::info("All compositor unit tests passed successfully!");
    return 0;
}
