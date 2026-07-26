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
    bool triggered = false;

    engine.set_shortcut_callback([&triggered](const std::string& shortcut) {
        if (shortcut == "launcher_toggle") {
            triggered = true;
        }
    });

    constexpr uint32_t MOD_CTRL = (1 << 2);
    constexpr uint32_t KEY_K = 37;

    // Test Ctrl+K press
    assert(engine.process_key_event(MOD_CTRL, KEY_K, true) == true);
    assert(triggered == true);

    std::cout << "[PASS] test_shortcut_engine\n";
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
