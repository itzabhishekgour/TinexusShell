#include "comp/window/window_state.hpp"
#include <cassert>
#include <iostream>

using namespace tinexus::comp;

void test_normal_to_maximized() {
    WindowBox initial_geom{50, 100, 800, 600};
    WindowStateMachine sm(initial_geom);

    assert(sm.state() == WindowState::Normal);
    assert(sm.geometry().normal_geom == initial_geom);
    assert(sm.geometry().current_geom == initial_geom);

    WindowBox work_area{0, 32, 1920, 1048};
    bool ok = sm.request_maximize(work_area);
    assert(ok);
    assert(sm.state() == WindowState::Maximized);
    assert(sm.geometry().current_geom == work_area);
    assert(sm.geometry().normal_geom == initial_geom);

    std::cout << "[PASS] normal_to_maximized" << std::endl;
}

void test_maximized_to_normal_exact_restore() {
    WindowBox initial_geom{120, 80, 1024, 768};
    WindowStateMachine sm(initial_geom);

    WindowBox work_area{0, 32, 1920, 1048};
    sm.request_maximize(work_area);
    assert(sm.state() == WindowState::Maximized);

    bool ok = sm.request_restore();
    assert(ok);
    assert(sm.state() == WindowState::Normal);
    assert(sm.geometry().current_geom == initial_geom);
    assert(sm.geometry().normal_geom == initial_geom);

    std::cout << "[PASS] maximized_to_normal_exact_restore" << std::endl;
}

void test_repeated_maximize_restore() {
    WindowBox initial_geom{200, 150, 640, 480};
    WindowStateMachine sm(initial_geom);
    WindowBox work_area{0, 46, 2560, 1394};

    for (int i = 0; i < 50; ++i) {
        bool max_ok = sm.request_maximize(work_area);
        assert(max_ok);
        assert(sm.state() == WindowState::Maximized);
        assert(sm.geometry().current_geom == work_area);
        assert(sm.geometry().normal_geom == initial_geom);

        bool rest_ok = sm.request_restore();
        assert(rest_ok);
        assert(sm.state() == WindowState::Normal);
        assert(sm.geometry().current_geom == initial_geom);
        assert(sm.geometry().normal_geom == initial_geom);
    }

    std::cout << "[PASS] repeated_maximize_restore" << std::endl;
}

void test_minimize_state_transition() {
    WindowBox initial_geom{100, 120, 900, 700};
    WindowStateMachine sm(initial_geom);

    // Normal -> Minimize
    bool min_ok = sm.request_minimize();
    assert(min_ok);
    assert(sm.state() == WindowState::Minimized);
    assert(sm.geometry().normal_geom == initial_geom);

    // Minimize -> Restore
    bool rest_ok = sm.request_restore();
    assert(rest_ok);
    assert(sm.state() == WindowState::Normal);
    assert(sm.geometry().current_geom == initial_geom);

    // Normal -> Maximize -> Minimize -> Restore
    WindowBox work_area{0, 32, 1920, 1048};
    sm.request_maximize(work_area);
    assert(sm.state() == WindowState::Maximized);

    min_ok = sm.request_minimize();
    assert(min_ok);
    assert(sm.state() == WindowState::Minimized);

    rest_ok = sm.request_restore();
    assert(rest_ok);
    assert(sm.state() == WindowState::Normal);
    assert(sm.geometry().current_geom == initial_geom);

    std::cout << "[PASS] minimize_state_transition" << std::endl;
}

int main() {
    test_normal_to_maximized();
    test_maximized_to_normal_exact_restore();
    test_repeated_maximize_restore();
    test_minimize_state_transition();
    return 0;
}
