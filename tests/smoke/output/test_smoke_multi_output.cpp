#include <cassert>
#include <iostream>
#include "comp/output/output_layout.hpp"
#include "wallpaper/wallpaper_provider.hpp"
#include "panel/panel_bar.hpp"

using namespace tinexus::comp;
using namespace tinexus::panel;
using namespace tinexus::wallpaper;

int main() {
    std::cout << "[+] Running smoke_multi_output test suite..." << std::endl;

    auto& layout = OutputLayout::instance();
    layout.add_output(OutputSpec{"HDMI-A-1", 0, 0, 1920, 1080, 60, 1.0f, 0, true, true});
    layout.add_output(OutputSpec{"DP-1", 1920, 0, 2560, 1440, 144, 1.0f, 0, true, false});

    assert(layout.outputs().size() == 2);

    // Render wallpaper buffers for both outputs
    ImageProvider provider;
    provider.load("/usr/share/backgrounds/tinexus.png");
    WallpaperBuffer buf1 = provider.render_buffer(1920, 1080);
    WallpaperBuffer buf2 = provider.render_buffer(2560, 1440);

    assert(buf1.width == 1920 && buf1.height == 1080);
    assert(buf2.width == 2560 && buf2.height == 1440);

    // Verify coordinate transform across multi-monitor boundary
    Point2D local_dp = layout.global_to_output_coords("DP-1", 2000, 500);
    assert(local_dp.x == 80); // 2000 - 1920 = 80
    assert(local_dp.y == 500);

    std::cout << "[+] smoke_multi_output: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
