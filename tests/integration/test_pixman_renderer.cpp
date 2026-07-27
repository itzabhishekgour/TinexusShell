#include <iostream>
#include <cassert>
#include <vector>
#include "common/logger.hpp"
#include "comp/renderer/pixman_renderer.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_pixman_renderer");
    tinexus::log::info("Running Pixman Software Renderer Integration Test...");

    tinexus::comp::PixmanRenderer renderer;
    assert(renderer.initialize(800, 600));

    // Create 100x100 RED pixel buffer (0xFFFF0000 in ARGB8888)
    std::vector<uint32_t> red_pixels(100 * 100, 0xFFFF0000);
    auto render_buf = std::make_shared<tinexus::comp::RenderBuffer>();
    render_buf->pixels = red_pixels.data();
    render_buf->width = 100;
    render_buf->height = 100;
    render_buf->stride = 400;
    render_buf->format = 0;

    tinexus::comp::RenderSurface surface(7007);
    surface.x = 50;
    surface.y = 50;
    surface.buffer = render_buf;

    // Execute rendering pass
    renderer.begin_frame();
    renderer.compose_surface(surface);
    renderer.end_frame();
    renderer.present();

    // Deterministically verify composite output pixel bytes
    const auto& canvas = renderer.canvas_buffer();
    size_t composited_idx = (50 * 800) + 50; // Pixel at (50, 50)
    assert(canvas[composited_idx] == 0xFFFF0000); // Verify exact red pixel byte match

    tinexus::log::info("Pixman Renderer compositing & deterministic pixel verification passed 100%!");
    return 0;
}
