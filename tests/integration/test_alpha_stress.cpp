#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/window/window_rules.hpp"
#include "comp/focus/focus_manager.hpp"
#include "comp/surface/surface_manager.hpp"

void test_window_rules_matching() {
    auto& rules = tinexus::comp::WindowRulesEngine::instance();

    tinexus::comp::WindowRule r1;
    r1.app_id = "org.mozilla.firefox";
    r1.workspace_name = "Web";
    r1.floating = false;
    rules.add_rule(r1);

    auto match = rules.match_rule("org.mozilla.firefox");
    assert(match.has_value());
    assert(match->workspace_name == "Web");
    assert(!match->floating.value());

    assert(!rules.match_rule("unknown.app").has_value());
    std::cout << "[PASS] test_window_rules_matching\n";
}

void test_focus_stress_toggling() {
    auto& focus = tinexus::comp::FocusManager::instance();

    for (uint64_t i = 1; i <= 50000; ++i) {
        if (i % 2 == 0) {
            focus.set_focus(tinexus::comp::FocusTargetType::Launcher, i, "tinexus-launcher");
            assert(focus.current_focus_type() == tinexus::comp::FocusTargetType::Launcher);
        } else {
            focus.set_focus(tinexus::comp::FocusTargetType::Window, i, "firefox");
            assert(focus.current_focus_type() == tinexus::comp::FocusTargetType::Window);
        }
    }

    std::cout << "[PASS] test_focus_stress_toggling (50,000 iterations)\n";
}

void test_surface_create_destroy_stress() {
    auto& surface_mgr = tinexus::comp::SurfaceManager::instance();

    for (uint32_t i = 1; i <= 1000; ++i) {
        uint32_t id = surface_mgr.create_surface(static_cast<pid_t>(1000 + i), "stress_app");
        assert(id > 0);
        assert(surface_mgr.transition_state(id, tinexus::comp::SurfaceState::Mapped));
        assert(surface_mgr.transition_state(id, tinexus::comp::SurfaceState::Destroyed));
    }

    std::cout << "[PASS] test_surface_create_destroy_stress (1,000 cycles)\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_alpha_stress");
    tinexus::log::info("Running Alpha Freeze Stress & Hardening Test Suite (v0.1.0-alpha.1)...");

    test_window_rules_matching();
    test_focus_stress_toggling();
    test_surface_create_destroy_stress();

    tinexus::log::info("All Alpha Freeze Stress & Hardening tests passed 100%!");
    return 0;
}
