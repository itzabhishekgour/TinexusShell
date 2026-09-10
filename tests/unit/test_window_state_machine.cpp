#include "comp/window/window_state.hpp"
#include <cassert>
#include <iostream>
#include <vector>

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

void test_arbitrary_resolutions_and_scales() {
    // Matrix of resolutions: 1280x720, 1366x768, 1920x1080, 2560x1440, 3840x2160
    const struct {
        int32_t w, h;
        double scale;
    } displays[] = {
        {1280, 720, 1.0},
        {1366, 768, 1.0},
        {1920, 1080, 1.0},
        {2560, 1440, 1.25},
        {3840, 2160, 1.5},
        {3840, 2160, 2.0}
    };

    for (const auto& disp : displays) {
        OutputGeometry out{};
        out.global_x = 0;
        out.global_y = 0;
        out.logical_width = disp.w;
        out.logical_height = disp.h;
        out.scale = disp.scale;

        // Top bar (32px), Dock (72px reservation)
        const int32_t top_reservation = 32;
        const int32_t dock_reservation = 72;
        out.work_x = 0;
        out.work_y = top_reservation;
        out.work_width = disp.w;
        out.work_height = disp.h - top_reservation - dock_reservation;
        out.sync_compat();

        WorkArea wa = out.work_area();
        assert(wa.x == 0);
        assert(wa.y == 32);
        assert(wa.width == disp.w);
        assert(wa.height == disp.h - 104);

        WindowStateMachine sm(WindowBox{50, 50, 400, 300});
        assert(sm.request_maximize(wa));
        assert(sm.state() == WindowState::Maximized);
        assert(sm.geometry().current_geom.x == wa.x);
        assert(sm.geometry().current_geom.y == wa.y);
        assert(sm.geometry().current_geom.width == wa.width);
        assert(sm.geometry().current_geom.height == wa.height);

        // Verify zero gap at bottom: window bottom == work area bottom
        const int32_t win_bottom = sm.geometry().current_geom.y + sm.geometry().current_geom.height;
        const int32_t wa_bottom = wa.y + wa.height;
        assert(win_bottom == wa_bottom);

        // Verify restore returns to exact floating box
        assert(sm.request_restore());
        assert(sm.geometry().current_geom == (WindowBox{50, 50, 400, 300}));
    }

    std::cout << "[PASS] arbitrary_resolutions_and_scales" << std::endl;
}

void test_layer_shell_anchor_permutations() {
    struct MockLayer {
        void* output{nullptr};
        uint32_t anchor{0};
        int32_t zone{0};
        int32_t margin_top{0};
        int32_t margin_bottom{0};
        int32_t margin_left{0};
        int32_t margin_right{0};
    };

    auto calc_work_area = [](int32_t out_w, int32_t out_h, const std::vector<MockLayer>& layers, void* target_out) {
        int32_t top_m = 0, bot_m = 0, left_m = 0, right_m = 0;
        for (const auto& l : layers) {
            if (l.output != target_out || l.zone <= 0) continue;
            const bool has_top = (l.anchor & 1) != 0;
            const bool has_bottom = (l.anchor & 2) != 0;
            const bool has_left = (l.anchor & 4) != 0;
            const bool has_right = (l.anchor & 8) != 0;

            if (has_top && has_bottom && has_left && has_right) continue;

            if (has_top && !has_bottom) top_m = std::max(top_m, l.zone + l.margin_top);
            else if (has_bottom && !has_top) bot_m = std::max(bot_m, l.zone + l.margin_bottom);
            else if (has_left && !has_right) left_m = std::max(left_m, l.zone + l.margin_left);
            else if (has_right && !has_left) right_m = std::max(right_m, l.zone + l.margin_right);
        }
        return WorkArea{left_m, top_m, std::max(0, out_w - left_m - right_m), std::max(0, out_h - top_m - bot_m)};
    };

    void* outA = reinterpret_cast<void*>(0x1000);

    // 1. TOP | LEFT | RIGHT (Aura / Top bar) - 32px
    {
        std::vector<MockLayer> layers = {{outA, 1 | 4 | 8, 32, 0, 0, 0, 0}};
        WorkArea wa = calc_work_area(1920, 1080, layers, outA);
        assert(wa == (WorkArea{0, 32, 1920, 1048}));
    }

    // 2. BOTTOM | LEFT | RIGHT (Dock) - 72px zone + 0 margin
    {
        std::vector<MockLayer> layers = {{outA, 2 | 4 | 8, 72, 0, 0, 0, 0}};
        WorkArea wa = calc_work_area(1920, 1080, layers, outA);
        assert(wa == (WorkArea{0, 0, 1920, 1008}));
    }

    // 3. Both TopBar (32) and Dock (72)
    {
        std::vector<MockLayer> layers = {
            {outA, 1 | 4 | 8, 32, 0, 0, 0, 0},
            {outA, 2 | 4 | 8, 72, 0, 0, 0, 0}
        };
        WorkArea wa = calc_work_area(1920, 1080, layers, outA);
        assert(wa == (WorkArea{0, 32, 1920, 976}));
    }

    // 4. Vertical Side Panels (LEFT=48px, RIGHT=64px)
    {
        std::vector<MockLayer> layers = {
            {outA, 4, 48, 0, 0, 0, 0},
            {outA, 8, 64, 0, 0, 0, 0}
        };
        WorkArea wa = calc_work_area(1920, 1080, layers, outA);
        assert(wa == (WorkArea{48, 0, 1920 - 48 - 64, 1080}));
    }

    // 5. Fullscreen Background/Overlay (TOP | BOTTOM | LEFT | RIGHT) - must NOT create edge reservations
    {
        std::vector<MockLayer> layers = {
            {outA, 1 | 2 | 4 | 8, 1080, 0, 0, 0, 0}
        };
        WorkArea wa = calc_work_area(1920, 1080, layers, outA);
        assert(wa == (WorkArea{0, 0, 1920, 1080}));
    }

    std::cout << "[PASS] layer_shell_anchor_permutations" << std::endl;
}

void test_multimonitor_isolation_and_negative_coords() {
    void* outA = reinterpret_cast<void*>(0x1000);
    void* outB = reinterpret_cast<void*>(0x2000);

    // Display A at (0, 0) 1920x1080
    OutputGeometry geomA{};
    geomA.output = outA;
    geomA.global_x = 0;
    geomA.global_y = 0;
    geomA.logical_width = 1920;
    geomA.logical_height = 1080;
    geomA.work_x = 0;
    geomA.work_y = 32;
    geomA.work_width = 1920;
    geomA.work_height = 976;
    geomA.sync_compat();

    // Display B at (1920, 0) 2560x1440 (No dock, purely secondary output)
    OutputGeometry geomB{};
    geomB.output = outB;
    geomB.global_x = 1920;
    geomB.global_y = 0;
    geomB.logical_width = 2560;
    geomB.logical_height = 1440;
    geomB.work_x = 1920;
    geomB.work_y = 0;
    geomB.work_width = 2560;
    geomB.work_height = 1440;
    geomB.sync_compat();

    // Verify isolation: Display B has full 1440 height, unaffected by Display A's dock
    assert(geomB.work_height == 1440);
    assert(geomB.work_width == 2560);
    assert(geomB.work_x == 1920);

    // Display C with negative X (-1920, 0)
    OutputGeometry geomC{};
    geomC.global_x = -1920;
    geomC.global_y = 0;
    geomC.logical_width = 1920;
    geomC.logical_height = 1080;
    geomC.work_x = -1920;
    geomC.work_y = 32;
    geomC.work_width = 1920;
    geomC.work_height = 976;
    geomC.sync_compat();

    assert(geomC.work_x == -1920);
    assert(geomC.work_y == 32);

    // Display D with negative Y (0, -1080)
    OutputGeometry geomD{};
    geomD.global_x = 0;
    geomD.global_y = -1080;
    geomD.logical_width = 1920;
    geomD.logical_height = 1080;
    geomD.work_x = 0;
    geomD.work_y = -1080 + 32;
    geomD.work_width = 1920;
    geomD.work_height = 976;
    geomD.sync_compat();

    assert(geomD.work_y == -1048);

    std::cout << "[PASS] multimonitor_isolation_and_negative_coords" << std::endl;
}

void test_restoration_permutations() {
    WindowBox initial_floating{150, 200, 750, 550};
    WindowStateMachine sm(initial_floating);
    WorkArea wa{0, 32, 1920, 976};

    // Flow 1: Normal -> Maximize -> Snap -> Restore
    assert(sm.request_maximize(wa));
    assert(sm.state() == WindowState::Maximized);
    assert(sm.geometry().normal_geom == initial_floating);

    assert(sm.request_snap(SnapMode::Right, wa));
    assert(sm.snap_mode() == SnapMode::Right);
    assert(sm.geometry().normal_geom == initial_floating);

    assert(sm.request_restore());
    assert(sm.state() == WindowState::Normal);
    assert(sm.snap_mode() == SnapMode::None);
    assert(sm.geometry().current_geom == initial_floating);

    // Flow 2: Normal -> Snap Left -> Snap TopRight -> Restore
    assert(sm.request_snap(SnapMode::Left, wa));
    assert(sm.geometry().normal_geom == initial_floating);
    assert(sm.request_snap(SnapMode::TopRight, wa));
    assert(sm.geometry().normal_geom == initial_floating);
    assert(sm.request_restore());
    assert(sm.geometry().current_geom == initial_floating);

    std::cout << "[PASS] restoration_permutations" << std::endl;
}

int main() {
    test_normal_to_maximized();
    test_maximized_to_normal_exact_restore();
    test_repeated_maximize_restore();
    test_minimize_state_transition();
    test_snapping_half_and_quarters();
    test_odd_resolution_snapping_exact_pixels();
    test_arbitrary_resolutions_and_scales();
    test_layer_shell_anchor_permutations();
    test_multimonitor_isolation_and_negative_coords();
    test_restoration_permutations();
    return 0;
}
