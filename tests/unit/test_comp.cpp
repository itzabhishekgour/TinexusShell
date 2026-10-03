#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/workspace/workspace_manager.hpp"
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

void test_workspace_gestures() {
    auto& mgr = tinexus::comp::WorkspaceManager::instance();
    mgr.initialize_default_workspaces(4);
    mgr.set_viewport_width(1000);

    assert(mgr.active_workspace_id() == 1);
    assert(!mgr.in_gesture());
    assert(mgr.current_slide_offset() == 0.0);

    // Case 1: Swipe Begin halts animation and initiates gesture mode
    mgr.begin_gesture_swipe();
    assert(mgr.in_gesture());

    // Case 2: Update gesture with 1:1 mapping (dx = -100 means swiping left -> slide offset increases to +100)
    mgr.update_gesture_swipe(-100.0, 100);
    assert(mgr.current_slide_offset() == 100.0);

    // Case 3: Distance threshold test (viewport = 1000, 25% threshold = 250px)
    // Drag further to 300px (dx = -200, cumulative = +300)
    mgr.update_gesture_swipe(-200.0, 1500); // long time, low velocity
    assert(mgr.current_slide_offset() == 300.0);

    // End gesture -> exceeded 25% threshold, should switch to Workspace 2!
    mgr.end_gesture_swipe(false);
    assert(!mgr.in_gesture());
    assert(mgr.active_workspace_id() == 2);
    assert(mgr.has_active_animation());

    // Step animation until settled
    for (int i = 0; i < 100 && mgr.has_active_animation(); ++i) {
        mgr.tick_animation(0.016);
    }
    assert(!mgr.has_active_animation());
    assert(std::abs(mgr.current_slide_offset() - 1000.0) < 1.0);

    // Case 4: Flick velocity test (macOS flick)
    // From Workspace 2 (offset 1000), flick towards Workspace 3 (dx = -50 in 20ms -> high velocity)
    mgr.begin_gesture_swipe();
    mgr.update_gesture_swipe(-10.0, 10);
    mgr.update_gesture_swipe(-50.0, 30); // 50px in 20ms = 2500 px/s velocity!
    mgr.end_gesture_swipe(false); // Even though travel is only 60px (< 250px), flick switches to WS 3!
    assert(mgr.active_workspace_id() == 3);

    for (int i = 0; i < 100 && mgr.has_active_animation(); ++i) {
        mgr.tick_animation(0.016);
    }
    assert(!mgr.has_active_animation());
    assert(std::abs(mgr.current_slide_offset() - 2000.0) < 1.0);

    // Case 5: Short drag + low velocity -> Snap back to original workspace
    // From Workspace 3 (offset 2000), drag only 50px towards WS 4 with low velocity
    mgr.begin_gesture_swipe();
    mgr.update_gesture_swipe(-50.0, 1000);
    mgr.end_gesture_swipe(false);
    // Travel was 50px (< 250px) and velocity was near 0 -> snaps back to WS 3!
    assert(mgr.active_workspace_id() == 3);

    for (int i = 0; i < 100 && mgr.has_active_animation(); ++i) {
        mgr.tick_animation(0.016);
    }
    assert(!mgr.has_active_animation());
    assert(std::abs(mgr.current_slide_offset() - 2000.0) < 1.0);

    // Case 6: Cancelled gesture -> Snap back to original workspace
    mgr.begin_gesture_swipe();
    mgr.update_gesture_swipe(-400.0, 100); // dragged past threshold
    mgr.end_gesture_swipe(true); // cancelled!
    assert(mgr.active_workspace_id() == 3); // Snaps back to 3!

    for (int i = 0; i < 100 && mgr.has_active_animation(); ++i) {
        mgr.tick_animation(0.016);
    }
    assert(!mgr.has_active_animation());
    assert(std::abs(mgr.current_slide_offset() - 2000.0) < 1.0);

    // Case 7: Boundary resistance (rubber banding)
    mgr.switch_workspace(1);
    for (int i = 0; i < 100 && mgr.has_active_animation(); ++i) {
        mgr.tick_animation(0.016);
    }
    // Now at WS 1 (offset 0), swipe fingers to the right (dx = +100 -> delta_offset = -100)
    mgr.begin_gesture_swipe();
    mgr.update_gesture_swipe(100.0, 100);
    // Resistance factor 0.35 applied since value < 0
    // Elastic limit clamp: 25% of viewport width (-250px)
    assert(mgr.current_slide_offset() < 0.0);
    assert(mgr.current_slide_offset() >= -250.0);
    mgr.end_gesture_swipe(false);
    assert(mgr.active_workspace_id() == 1); // Snaps back to 1

    std::cout << "[PASS] test_workspace_gestures (1:1 Tracking, Flick, Threshold & Rubber-banding Verified)\n";
}

void test_window_focus_manager() {
    auto& focus_mgr = tinexus::comp::FocusManager::instance();

    focus_mgr.set_focus(tinexus::comp::FocusTargetType::Window, 1234, "org.mozilla.firefox");
    assert(focus_mgr.current_surface_id() == 1234);
    assert(focus_mgr.current_target_id() == "org.mozilla.firefox");
    assert(focus_mgr.current_focus_type() == tinexus::comp::FocusTargetType::Window);

    focus_mgr.set_focus(tinexus::comp::FocusTargetType::Launcher, 0, "tinexus-launcher");
    assert(focus_mgr.current_focus_type() == tinexus::comp::FocusTargetType::Launcher);
    assert(focus_mgr.current_target_id() == "tinexus-launcher");

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

    // Bare Super/Meta key is passed through to allow Super+1..9 modifier combinations
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_NONE, KEY_LEFTMETA, true, 0) == false);
    assert(last_shortcut.empty());
    last_shortcut.clear();
    assert(engine.process_key_event(MOD_NONE, KEY_RIGHTMETA, true, 0) == false);
    assert(last_shortcut.empty());

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
    test_workspace_gestures();
    test_window_focus_manager();
    test_shell_state();
    test_frame_scheduler();
    test_shortcut_engine();

    tinexus::log::info("All compositor unit tests passed successfully!");
    return 0;
}
