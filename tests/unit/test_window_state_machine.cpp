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

    WorkArea work_area{0, 32, 1920, 1048};
    bool ok = sm.request_maximize(work_area);
    assert(ok);
    assert(sm.state() == WindowState::Maximized);
    assert(sm.geometry().current_geom.width == work_area.width);
    assert(sm.geometry().current_geom.height == work_area.height);
    assert(sm.geometry().normal_geom == initial_geom);

    std::cout << "[PASS] normal_to_maximized" << std::endl;
}

void test_maximized_to_normal_exact_restore() {
    WindowBox initial_geom{120, 80, 1024, 768};
    WindowStateMachine sm(initial_geom);

    WorkArea work_area{0, 32, 1920, 1048};
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
    WorkArea work_area{0, 46, 2560, 1394};

    for (int i = 0; i < 50; ++i) {
        bool max_ok = sm.request_maximize(work_area);
        assert(max_ok);
        assert(sm.state() == WindowState::Maximized);
        assert(sm.geometry().current_geom.width == work_area.width);
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
    WorkArea work_area{0, 32, 1920, 1048};
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

void test_snapping_half_and_quarters() {
    WindowBox initial_geom{80, 80, 800, 600};
    WindowStateMachine sm(initial_geom);
    WorkArea work_area{0, 32, 1920, 960}; // 1920 width, 960 height usable

    // 1. Snap Left (50% width, full height)
    bool ok = sm.request_snap(SnapMode::Left, work_area);
    assert(ok);
    assert(sm.snap_mode() == SnapMode::Left);
    assert(sm.geometry().current_geom == (WindowBox{0, 32, 960, 960}));
    assert(sm.geometry().normal_geom == initial_geom);

    // Restore to normal
    sm.request_restore();
    assert(sm.snap_mode() == SnapMode::None);
    assert(sm.geometry().current_geom == initial_geom);

    // 2. Snap Right (50% width, full height)
    ok = sm.request_snap(SnapMode::Right, work_area);
    assert(ok);
    assert(sm.snap_mode() == SnapMode::Right);
    assert(sm.geometry().current_geom == (WindowBox{960, 32, 960, 960}));

    // 3. Snap TopLeft (25% quarter)
    ok = sm.request_snap(SnapMode::TopLeft, work_area);
    assert(ok);
    assert(sm.snap_mode() == SnapMode::TopLeft);
    assert(sm.geometry().current_geom == (WindowBox{0, 32, 960, 480}));

    // 4. Snap TopRight (25% quarter)
    ok = sm.request_snap(SnapMode::TopRight, work_area);
    assert(ok);
    assert(sm.snap_mode() == SnapMode::TopRight);
    assert(sm.geometry().current_geom == (WindowBox{960, 32, 960, 480}));

    // 5. Snap BottomLeft (25% quarter)
    ok = sm.request_snap(SnapMode::BottomLeft, work_area);
    assert(ok);
    assert(sm.snap_mode() == SnapMode::BottomLeft);
    assert(sm.geometry().current_geom == (WindowBox{0, 512, 960, 480}));

    // 6. Snap BottomRight (25% quarter)
    ok = sm.request_snap(SnapMode::BottomRight, work_area);
    assert(ok);
    assert(sm.snap_mode() == SnapMode::BottomRight);
    assert(sm.geometry().current_geom == (WindowBox{960, 512, 960, 480}));

    // Exact restore verification
    sm.request_restore();
    assert(sm.geometry().current_geom == initial_geom);

    std::cout << "[PASS] snapping_half_and_quarters" << std::endl;
}

void test_odd_resolution_snapping_exact_pixels() {
    // 1366x768 odd display test fixture
    WorkArea work_area{0, 30, 1366, 650};
    auto left_box = WindowStateMachine::compute_snap_box(SnapMode::Left, work_area);
    auto right_box = WindowStateMachine::compute_snap_box(SnapMode::Right, work_area);

    assert(left_box.width == 683);
    assert(right_box.width == 683);
    assert(left_box.width + right_box.width == work_area.width);
    assert(right_box.x == left_box.x + left_box.width);

    // 1920x1080 with 32 top and 72 bottom dock
    WorkArea hd_area{0, 32, 1920, 976};
    auto tl = WindowStateMachine::compute_snap_box(SnapMode::TopLeft, hd_area);
    auto tr = WindowStateMachine::compute_snap_box(SnapMode::TopRight, hd_area);
    auto bl = WindowStateMachine::compute_snap_box(SnapMode::BottomLeft, hd_area);
    auto br = WindowStateMachine::compute_snap_box(SnapMode::BottomRight, hd_area);

    assert(tl.width + tr.width == 1920);
    assert(bl.width + br.width == 1920);
    assert(tl.height + bl.height == 976);
    assert(tr.height + br.height == 976);

    std::cout << "[PASS] odd_resolution_snapping_exact_pixels" << std::endl;
}

int main() {
    test_normal_to_maximized();
    test_maximized_to_normal_exact_restore();
    test_repeated_maximize_restore();
    test_minimize_state_transition();
    test_snapping_half_and_quarters();
    test_odd_resolution_snapping_exact_pixels();
    return 0;
}
