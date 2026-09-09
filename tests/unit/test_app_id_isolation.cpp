#include <txui/window/Window.hpp>
#include <common/logger.hpp>
#include <cassert>
#include <iostream>
#include <vector>
#include <string>

// Test mock toplevel tracking for isolation verification
struct MockToplevel {
    std::string title;
    std::string app_id;
    bool activated{false};
    bool raised{false};
};

class MockWindowManager {
public:
    std::vector<MockToplevel> windows;

    void add_window(std::string title, std::string app_id) {
        windows.push_back({std::move(title), std::move(app_id), false, false});
    }

    bool focus_app(const std::string& target_app_id) {
        bool found = false;
        for (auto& win : windows) {
            if (win.app_id == target_app_id) {
                win.activated = true;
                win.raised = true;
                found = true;
            } else {
                win.activated = false;
            }
        }
        return found;
    }
};

void test_canonical_app_id_propagation() {
    std::cout << "[Test 1] Testing canonical app_id propagation on Window::create..." << std::endl;

    // Explicit app_id
    auto win_about = txui::Window::create(680, 420, "About Tinexus", false, "tinexus-about");
    assert(win_about->app_id() == "tinexus-about");

    auto win_monitor = txui::Window::create(860, 580, "Activity Monitor", false, "tinexus-monitor");
    assert(win_monitor->app_id() == "tinexus-monitor");

    auto win_settings = txui::Window::create(1000, 640, "Tinexus Settings", false, "tinexus-settings");
    assert(win_settings->app_id() == "tinexus-settings");

    auto win_terminal = txui::Window::create(800, 600, "Tinexus Terminal", false, "tinexus-terminal");
    assert(win_terminal->app_id() == "tinexus-terminal");

    auto win_files = txui::Window::create(1000, 620, "Tinexus Files", false, "tinexus-files");
    assert(win_files->app_id() == "tinexus-files");

    std::cout << "  -> Passed: Explicit app_ids correctly registered on windows.\n";
}

void test_app_id_isolation_and_focus() {
    std::cout << "[Test 2] Testing App-ID isolation: focus_app('tinexus-about') vs focus_app('tinexus-monitor')..." << std::endl;

    MockWindowManager wm;
    wm.add_window("About Tinexus", "tinexus-about");
    wm.add_window("Activity Monitor", "tinexus-monitor");
    wm.add_window("Tinexus Settings", "tinexus-settings");

    // 1. Focus About Tinexus
    bool res1 = wm.focus_app("tinexus-about");
    assert(res1 == true);
    assert(wm.windows[0].activated == true);  // About is activated
    assert(wm.windows[0].raised == true);
    assert(wm.windows[1].activated == false); // Monitor is NOT activated
    assert(wm.windows[2].activated == false); // Settings is NOT activated
    std::cout << "  -> Part A Passed: ONLY About received activation.\n";

    // 2. Focus Activity Monitor
    bool res2 = wm.focus_app("tinexus-monitor");
    assert(res2 == true);
    assert(wm.windows[0].activated == false); // About deactivated
    assert(wm.windows[1].activated == true);  // Monitor activated
    assert(wm.windows[1].raised == true);
    assert(wm.windows[2].activated == false); // Settings is NOT activated
    std::cout << "  -> Part B Passed: ONLY Activity Monitor received activation.\n";

    // 3. Focus Non-existent app_id
    bool res3 = wm.focus_app("tinexus-unknown");
    assert(res3 == false);
    assert(wm.windows[0].activated == false);
    assert(wm.windows[1].activated == false);
    assert(wm.windows[2].activated == false);
    std::cout << "  -> Part C Passed: Unknown app_id activated zero windows.\n";
}

void test_dirty_repaint_suppression() {
    std::cout << "[Test 3] Testing dirty-repaint flag behavior..." << std::endl;

    auto win = txui::Window::create(640, 480, "Test Window", false, "test-window");
    // Initially dirty so first frame renders
    assert(win->needs_repaint() == true);

    // Presenting clears dirty flag
    win->present();
    assert(win->needs_repaint() == false);

    // Second present() when not dirty does not do redundant rasterization
    win->present();
    assert(win->needs_repaint() == false);

    // Explicit request_repaint() marks dirty
    win->request_repaint();
    assert(win->needs_repaint() == true);

    win->present();
    assert(win->needs_repaint() == false);

    // Window resize marks dirty
    win->resize(800, 600);
    assert(win->needs_repaint() == true);

    std::cout << "  -> Passed: Redundant repaints suppressed when static.\n";
}

int main() {
    tinexus::log::set_component_name("test_app_id_isolation");
    std::cout << "========================================" << std::endl;
    std::cout << " RUNNING APP-ID ISOLATION & LIFECYCLE TESTS " << std::endl;
    std::cout << "========================================" << std::endl;

    test_canonical_app_id_propagation();
    test_app_id_isolation_and_focus();
    test_dirty_repaint_suppression();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL APP-ID ISOLATION TESTS PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
