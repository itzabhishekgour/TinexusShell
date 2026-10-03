#include <cassert>
#include <iostream>
#include <vector>
#include <txui/window/Window.hpp>
#include <txui/render/CanvasRenderTarget.hpp>

using namespace txui;

void test_arbitrary_output_width_adaptation() {
    std::cout << "[TEST] Running Topbar Geometry Dynamic Output Adaptation Test...\n";

    // Test across a diverse matrix of display resolutions
    struct DisplayMode {
        const char* name;
        uint32_t width;
        uint32_t height;
    };

    const std::vector<DisplayMode> test_outputs = {
        {"1080p FHD (ASUS)", 1920, 1080},
        {"1440p QHD",        2560, 1440},
        {"4K UHD",           3840, 2160},
        {"HD Laptop",        1366, 768},
        {"16:10 Laptop",     1920, 1200},
        {"WXGA Display",     1280, 800},
        {"Ultrawide Display",3440, 1440}
    };

    for (const auto& out : test_outputs) {
        // Initial shell window requested at height 46 with layer_shell = true
        auto win = Window::create(800, 46, "Aura", /*layer_shell=*/true);
        assert(win != nullptr);

        // Configure as Top layer with Top | Left | Right anchors and exclusive zone 32
        win->set_layer_shell_config(
            LayerType::Top,
            LayerAnchor::Top | LayerAnchor::Left | LayerAnchor::Right,
            32
        );

        // Verify that anchors are correctly stored and tracked
        assert((win->layer_anchors() & LayerAnchor::Left) != 0);
        assert((win->layer_anchors() & LayerAnchor::Right) != 0);
        assert((win->layer_anchors() & LayerAnchor::Top) != 0);

        // Simulate compositor dispatching layer-shell configure event based on target output width
        win->on_configure(out.width, 46);

        // 1. Authoritative verification: window width must exactly equal target output width
        assert(win->width() == out.width);
        assert(win->height() == 46);

        // 2. Window needs repaint on initial configure
        assert(win->needs_repaint());

        // 3. Redundant configure with same dimensions must NOT mark repaint or trigger loop
        win->present(); // Clears needs_repaint
        win->on_configure(out.width, 46);
        assert(!win->needs_repaint());

        std::cout << "  [PASS] Output '" << out.name << "' (" << out.width << "x" << out.height
                  << ") -> Topbar dynamically configured to full width: " << win->width() << "px\n";
    }

    std::cout << "[PASS] All dynamic output geometry tests succeeded with ZERO hardcoding!\n";
}

int main() {
    test_arbitrary_output_width_adaptation();
    return 0;
}
