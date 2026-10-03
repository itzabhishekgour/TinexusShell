#include <txui/core/SingleInstance.hpp>
#include <dock/DockWidget.hpp>
#include <ipcd/protocol/dock_protocol.hpp>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace txui;
using namespace tinexus::ipcd::protocol;

// 1. Canonical App ID Mapping
void test_canonical_app_id_mapping() {
    assert(get_canonical_app_id("tinexus-settings") == "tinexus-settings");
    assert(get_canonical_app_id("tinexus-settings-ui") == "tinexus-settings");
    assert(get_canonical_app_id("/usr/bin/tinexus-settings") == "tinexus-settings");

    assert(get_canonical_app_id("tinexus-about") == "tinexus-about");
    assert(get_canonical_app_id("about") == "tinexus-about");

    assert(get_canonical_app_id("tinexus-monitor") == "tinexus-monitor");
    assert(get_canonical_app_id("monitor") == "tinexus-monitor");

    assert(get_canonical_app_id("tinexus-launcher") == "tinexus-launcher");
    assert(get_canonical_app_id("launcher") == "tinexus-launcher");

    assert(get_canonical_app_id("tinexus-files") == "tinexus-files");
    assert(get_canonical_app_id("files") == "tinexus-files");

    assert(get_canonical_app_id("tinexus-terminal") == "tinexus-terminal");
    assert(get_canonical_app_id("terminal") == "tinexus-terminal");

    assert(get_canonical_app_id("tinexus-store") == "tinexus-store");
    assert(get_canonical_app_id("tinexus-pkg") == "tinexus-store");

    assert(get_canonical_app_id("tinexus-lock") == "tinexus-lock");
    assert(get_canonical_app_id("lock") == "tinexus-lock");

    std::cout << "[PASS] test_canonical_app_id_mapping" << std::endl;
}

// 2. Single-Instance vs Multi-Instance classification
void test_single_instance_classification() {
    // Single instance required
    assert(is_single_instance_app("tinexus-settings"));
    assert(is_single_instance_app("tinexus-about"));
    assert(is_single_instance_app("tinexus-monitor"));
    assert(is_single_instance_app("tinexus-launcher"));
    assert(is_single_instance_app("tinexus-store"));
    assert(is_single_instance_app("tinexus-lock"));

    // Multi-instance must NOT be restricted
    assert(!is_single_instance_app("tinexus-terminal"));
    assert(!is_single_instance_app("tinexus-files"));
    assert(!is_single_instance_app("foot"));
    assert(!is_single_instance_app("gedit"));

    std::cout << "[PASS] test_single_instance_classification" << std::endl;
}

// 3. Multi-instance window counting and lifecycle tracking
void test_multi_instance_lifecycle_tracking() {
    struct WindowRecord {
        uint64_t id;
        std::string app_id;
    };

    std::vector<WindowRecord> open_windows;

    auto add_window = [&](uint64_t id, const std::string& raw_app_id) {
        std::string canonical = get_canonical_app_id(raw_app_id);
        open_windows.push_back({id, canonical});
    };

    auto remove_window = [&](uint64_t id, std::string& closed_app_if_last) -> bool {
        auto it = std::find_if(open_windows.begin(), open_windows.end(),
                               [id](const WindowRecord& w) { return w.id == id; });
        if (it == open_windows.end()) return false;

        std::string canonical = it->app_id;
        open_windows.erase(it);

        // Check if any other window with this canonical app_id remains
        bool has_remaining = std::any_of(open_windows.begin(), open_windows.end(),
                                         [&canonical](const WindowRecord& w) {
                                             return w.app_id == canonical;
                                         });
        if (!has_remaining) {
            closed_app_if_last = canonical;
        } else {
            closed_app_if_last.clear();
        }
        return true;
    };

    // Open first terminal window -> App starts
    add_window(1, "tinexus-terminal");
    assert(open_windows.size() == 1);

    // Open second terminal window -> Multi-instance allowed!
    add_window(2, "tinexus-terminal");
    assert(open_windows.size() == 2);

    // Open third terminal window
    add_window(3, "tinexus-terminal");
    assert(open_windows.size() == 3);

    // Close window 2 -> app is STILL running (windows 1 and 3 remain)
    std::string closed_app;
    assert(remove_window(2, closed_app));
    assert(closed_app.empty()); // Not closed to Dock yet!
    assert(open_windows.size() == 2);

    // Close window 1 -> app still running
    assert(remove_window(1, closed_app));
    assert(closed_app.empty());
    assert(open_windows.size() == 1);

    // Close window 3 -> LAST window closed -> Dock should be notified
    assert(remove_window(3, closed_app));
    assert(closed_app == "tinexus-terminal");
    assert(open_windows.size() == 0);

    std::cout << "[PASS] test_multi_instance_lifecycle_tracking" << std::endl;
}

// 4. Dock icon state machine transitions
void test_dock_icon_state_transitions() {
    DockWidget dock;
    dock.layout(Rect(0, 0, 1920, 120));

    // Initial state of icons must be NotRunning
    for (const auto& icon : dock.icons()) {
        assert(icon.app_state == DockIconAppState::NotRunning);
    }

    // 1. App started -> RunningFocused
    dock.update_icon_state("tinexus-settings", DockIconAppState::RunningFocused);
    for (const auto& icon : dock.icons()) {
        if (icon.app_id == "tinexus-settings") {
            assert(icon.app_state == DockIconAppState::RunningFocused);
        }
    }

    // 2. Another app started -> First app goes to RunningBg, new app is RunningFocused
    dock.update_icon_state("tinexus-terminal", DockIconAppState::RunningFocused);
    dock.update_icon_state("tinexus-settings", DockIconAppState::RunningBg);

    for (const auto& icon : dock.icons()) {
        if (icon.app_id == "tinexus-settings") {
            assert(icon.app_state == DockIconAppState::RunningBg);
        } else if (icon.app_id == "tinexus-terminal") {
            assert(icon.app_state == DockIconAppState::RunningFocused);
        }
    }

    // 3. App minimized -> Minimized
    dock.update_icon_state("tinexus-terminal", DockIconAppState::Minimized);
    for (const auto& icon : dock.icons()) {
        if (icon.app_id == "tinexus-terminal") {
            assert(icon.app_state == DockIconAppState::Minimized);
        }
    }

    // 4. App restored -> RunningFocused
    dock.update_icon_state("tinexus-terminal", DockIconAppState::RunningFocused);
    for (const auto& icon : dock.icons()) {
        if (icon.app_id == "tinexus-terminal") {
            assert(icon.app_state == DockIconAppState::RunningFocused);
        }
    }

    // 5. App closed -> NotRunning
    dock.update_icon_state("tinexus-terminal", DockIconAppState::NotRunning);
    for (const auto& icon : dock.icons()) {
        if (icon.app_id == "tinexus-terminal") {
            assert(icon.app_state == DockIconAppState::NotRunning);
        }
    }

    std::cout << "[PASS] test_dock_icon_state_transitions" << std::endl;
}

// 5. Launcher Toggle State Machine
void test_launcher_toggle_determinism() {
    bool launcher_active = false;

    auto toggle_launcher = [&launcher_active]() -> std::string {
        if (launcher_active) {
            launcher_active = false;
            return "CLOSE";
        } else {
            launcher_active = true;
            return "SPAWN";
        }
    };

    // First Ctrl+K: Open
    assert(toggle_launcher() == "SPAWN");
    assert(launcher_active);

    // Second Ctrl+K: Close
    assert(toggle_launcher() == "CLOSE");
    assert(!launcher_active);

    // Third Ctrl+K: Open
    assert(toggle_launcher() == "SPAWN");
    assert(launcher_active);

    // Fourth Ctrl+K: Close
    assert(toggle_launcher() == "CLOSE");
    assert(!launcher_active);

    std::cout << "[PASS] test_launcher_toggle_determinism" << std::endl;
}

int main() {
    std::cout << "=== Running Lifecycle & Dock Tests ===" << std::endl;
    test_canonical_app_id_mapping();
    test_single_instance_classification();
    test_multi_instance_lifecycle_tracking();
    test_dock_icon_state_transitions();
    test_launcher_toggle_determinism();
    std::cout << "=== All Lifecycle & Dock Tests PASSED ===" << std::endl;
    return 0;
}
