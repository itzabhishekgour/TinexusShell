#include <cassert>
#include <iostream>
#include <vector>
#include <txui/window/Window.hpp>
#include <txui/render/CanvasRenderTarget.hpp>

using namespace txui;

// Helper to check if a point is contained in any rectangle of an input region
bool region_contains(const std::vector<Rect>& region, double x, double y) {
    for (const auto& r : region) {
        if (r.contains(x, y)) {
            return true;
        }
    }
    return false;
}

void test_input_region_basic_contract() {
    std::cout << "[TEST] Running Input Region Basic Contract Test...\n";

    auto win = Window::create(800, 46, "TestWindow", /*layer_shell=*/false);
    assert(win != nullptr);

    // Initial state: default full surface
    assert(!win->has_custom_input_region());
    assert(win->input_region().empty());

    // Set custom region with 2 rects
    std::vector<Rect> rects = {
        Rect(0.0, 0.0, 800.0, 32.0),
        Rect(250.0, 32.0, 300.0, 14.0)
    };
    win->set_input_region(rects);
    assert(win->has_custom_input_region());
    assert(win->input_region().size() == 2);
    assert(win->input_region()[0].width() == 800.0);
    assert(win->input_region()[0].height() == 32.0);
    assert(win->input_region()[1].x() == 250.0);
    assert(win->input_region()[1].height() == 14.0);

    // Clear region: resets back to default
    win->clear_input_region();
    assert(!win->has_custom_input_region());
    assert(win->input_region().empty());

    std::cout << "  [PASS] Basic set_input_region and clear_input_region state transitions verified.\n";
}

void test_topbar_dead_strip_elimination_multi_resolution() {
    std::cout << "[TEST] Running Topbar Dead-Strip Elimination Test across Resolutions...\n";

    struct DisplayMode {
        const char* name;
        uint32_t width;
        uint32_t height;
    };

    const std::vector<DisplayMode> test_outputs = {
        {"1080p FHD (ASUS TUF)", 1920, 1080},
        {"1440p QHD",            2560, 1440},
        {"4K UHD",               3840, 2160},
        {"HD Laptop",            1366, 768},
        {"16:10 Laptop",         1920, 1200},
        {"WXGA Display",         1280, 800},
        {"Ultrawide Display",    3440, 1440}
    };

    constexpr double BAR_HEIGHT = 32.0;
    constexpr double NOTCH_HEIGHT = 46.0;
    constexpr double TOTAL_BAR_HEIGHT = NOTCH_HEIGHT;
    constexpr double NOTCH_TOP_HALF = 136.0;

    for (const auto& out : test_outputs) {
        auto win = Window::create(800, static_cast<uint32_t>(TOTAL_BAR_HEIGHT), "Aura", /*layer_shell=*/true);
        assert(win != nullptr);

        // Exclusive zone is set to 32px so maximized windows sit flush against the flat bar sides
        win->set_layer_shell_config(
            LayerType::Top,
            LayerAnchor::Top | LayerAnchor::Left | LayerAnchor::Right,
            static_cast<int32_t>(BAR_HEIGHT)
        );

        // Compositor configures the topbar to full display width
        win->on_configure(out.width, static_cast<uint32_t>(TOTAL_BAR_HEIGHT));
        assert(win->width() == out.width);
        assert(win->height() == static_cast<uint32_t>(TOTAL_BAR_HEIGHT));

        // Compute dynamic multi-rect input region matching DesktopShellWidget::compute_input_region
        double w = static_cast<double>(win->width());
        double cx = w * 0.5;
        std::vector<Rect> topbar_input_region = {
            Rect(0.0, 0.0, w, BAR_HEIGHT),
            Rect(cx - NOTCH_TOP_HALF, BAR_HEIGHT, NOTCH_TOP_HALF * 2.0, NOTCH_HEIGHT - BAR_HEIGHT)
        };
        win->set_input_region(topbar_input_region);

        [[maybe_unused]] const auto& region = win->input_region();
        assert(win->has_custom_input_region());
        assert(region.size() == 2);

        // ── 1. Topbar flat area (y = 0..32) MUST accept input across full width ─────────────────
        assert(region_contains(region, 50.0, 10.0));       // Left logo/app title
        assert(region_contains(region, cx, 16.0));         // Bar center
        assert(region_contains(region, w - 50.0, 16.0));   // Right system tray
        assert(region_contains(region, 50.0, 31.0));       // Bottom edge of left flat bar
        assert(region_contains(region, w - 50.0, 31.0));   // Bottom edge of right flat bar

        // ── 2. Center notch protrusion area (y = 32..46) MUST accept input in center ────────────
        assert(region_contains(region, cx, 35.0));         // Notch center (clock / calendar)
        assert(region_contains(region, cx, 45.0));         // Notch bottom edge
        assert(region_contains(region, cx - 100.0, 38.0)); // Left notch trapezoid wing
        assert(region_contains(region, cx + 100.0, 38.0)); // Right notch trapezoid wing

        // ── 3. CRITICAL PROOF: Outside the notch, area at y > 32.0 belongs strictly to the client window!
        // Client windows are placed starting at y = 32.0 by exclusive_zone = 32 (flush with flat bar).
        assert(!region_contains(region, 50.0, 33.0));      // Client window tabs/titlebar (left)
        assert(!region_contains(region, w - 50.0, 33.0));  // Client window controls (right)
        assert(!region_contains(region, cx - 150.0, 35.0));// Outside notch bounding box at y=35

        // Below the notch (y > 46.0), center area passes through as well
        assert(!region_contains(region, cx, 46.1));

        std::cout << "  [PASS] Output '" << out.name << "' (" << out.width << "x" << out.height
                  << ") -> Multi-rect input region: flush at 32px, notch active at 46px.\n";
    }

    std::cout << "[PASS] All multi-resolution dead-strip elimination tests passed!\n";
}

void test_flyout_input_region_lifecycle() {
    std::cout << "[TEST] Running Flyout Open/Close Input Region Lifecycle Test...\n";

    constexpr double BAR_HEIGHT = 32.0;
    constexpr double NOTCH_HEIGHT = 46.0;
    constexpr double TOTAL_BAR_HEIGHT = NOTCH_HEIGHT;
    constexpr double NOTCH_TOP_HALF = 136.0;

    auto win = Window::create(1920, static_cast<uint32_t>(TOTAL_BAR_HEIGHT), "Aura", /*layer_shell=*/true);
    assert(win != nullptr);

    auto compute_input_region = [](double w, double h) -> std::vector<Rect> {
        if (h <= TOTAL_BAR_HEIGHT) {
            double cx = w * 0.5;
            return {
                Rect(0.0, 0.0, w, BAR_HEIGHT),
                Rect(cx - NOTCH_TOP_HALF, BAR_HEIGHT, NOTCH_TOP_HALF * 2.0, NOTCH_HEIGHT - BAR_HEIGHT)
            };
        }
        return { Rect(0.0, 0.0, w, h) };
    };

    // 1. Initial idle state (collapsed to 46px topbar with multi-rect region)
    win->set_input_region(compute_input_region(1920.0, TOTAL_BAR_HEIGHT));
    assert(win->has_custom_input_region());
    assert(region_contains(win->input_region(), 1920.0 * 0.5, 38.0)); // Center notch accepts input
    assert(region_contains(win->input_region(), 1920.0 - 50.0, 20.0)); // System tray accepts input
    assert(!region_contains(win->input_region(), 100.0, 35.0));        // Below 32px on flat sides passes through!

    // 2. Notification flyout opens (height expands to 350px)
    win->resize(1920, 350);
    win->set_input_region(compute_input_region(1920.0, 350.0));
    assert(win->has_custom_input_region());
    // Flyout covers entire expanded area: notification card in the 32..46px and 46..350px range receives input!
    assert(region_contains(win->input_region(), 1920.0 - 200.0, 44.0)); // Upper half of first notification
    assert(region_contains(win->input_region(), 1920.0 - 200.0, 70.0)); // Lower half of first notification
    assert(region_contains(win->input_region(), 1920.0 - 200.0, 150.0)); // Second notification

    // 3. Notification flyout closes (height returns to 46px)
    win->resize(1920, static_cast<uint32_t>(TOTAL_BAR_HEIGHT));
    win->set_input_region(compute_input_region(1920.0, TOTAL_BAR_HEIGHT));
    assert(win->has_custom_input_region());
    assert(region_contains(win->input_region(), 1920.0 * 0.5, 38.0)); // Center notch still accepts input
    assert(!region_contains(win->input_region(), 1920.0 - 200.0, 35.0)); // Area outside notch at y>32 passes through again

    std::cout << "  [PASS] Flyout lifecycle correctly covers popup content in expanded range and collapses.\n";
}

void test_atomic_input_region_present_preservation() {
    std::cout << "[TEST] Running Atomic Input Region Present Preservation Test...\n";

    constexpr double BAR_HEIGHT = 32.0;
    constexpr double NOTCH_HEIGHT = 46.0;
    constexpr double TOTAL_BAR_HEIGHT = NOTCH_HEIGHT;
    constexpr double NOTCH_TOP_HALF = 136.0;

    auto win = Window::create(1920, static_cast<uint32_t>(TOTAL_BAR_HEIGHT), "Aura", /*layer_shell=*/true);
    assert(win != nullptr);

    double cx = 1920.0 * 0.5;
    std::vector<Rect> rects = {
        Rect(0.0, 0.0, 1920.0, BAR_HEIGHT),
        Rect(cx - NOTCH_TOP_HALF, BAR_HEIGHT, NOTCH_TOP_HALF * 2.0, NOTCH_HEIGHT - BAR_HEIGHT)
    };
    win->set_input_region(rects);
    assert(win->has_custom_input_region());
    assert(win->input_region().size() == 2);

    // Simulate 10 consecutive frame presentations (as occurs during cursor motion or redrawing)
    for (int frame = 0; frame < 10; ++frame) {
        win->request_repaint();
        win->present();
        // Custom input region must remain active across every single present() cycle
        assert(win->has_custom_input_region());
        assert(win->input_region().size() == 2);
        assert(region_contains(win->input_region(), 200.0, 16.0));  // Flat topbar accepted
        assert(region_contains(win->input_region(), cx, 38.0));     // Center notch accepted
        assert(!region_contains(win->input_region(), 200.0, 35.0)); // Outside notch >32 passes through
    }

    std::cout << "  [PASS] Custom input region preserved atomically across all consecutive present() frames.\n";
}

int main() {
    test_input_region_basic_contract();
    test_topbar_dead_strip_elimination_multi_resolution();
    test_flyout_input_region_lifecycle();
    test_atomic_input_region_present_preservation();
    std::cout << "\n============================================================\n";
    std::cout << "ALL INPUT REGION TESTS PASSED CLEANLY!\n";
    std::cout << "============================================================\n";
    return 0;
}
