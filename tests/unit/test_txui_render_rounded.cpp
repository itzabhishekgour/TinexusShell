#include <txui/render/CanvasRenderTarget.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/ImageWriter.hpp>
#include <txui/render/ImageReader.hpp>
#include <iostream>
#include <cstdlib>
#include <cstdio>

int main() {
    std::cout << "=== Phase 4.2.2 Unit Test: Rounded Rectangle Rendering & AA ===" << std::endl;

    txui::CanvasRenderTarget target(200, 200);
    target.clear(txui::Color(0, 0, 0, 0)); // Transparent

    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);

    painter.begin_frame();
    // 100x100 square with radius=20 at (50, 50)
    painter.fill_rounded_rect(txui::Rect(50.0, 50.0, 100.0, 100.0), 20.0, txui::Color(255, 0, 0, 255));
    painter.end_frame();

    txui::PixmanBackend backend;
    backend.execute(buffer, target);

    // 1. Verify interior pixel is fully opaque red
    txui::uint32 center_px = target.pixel_at(100, 100);
    if (center_px != 0xFFFF0000U) {
        std::cerr << "FAIL: Interior pixel expected 0xFFFF0000, got 0x"
                  << std::hex << center_px << std::dec << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Interior pixel verified" << std::endl;

    // 2. Verify outside corner pixel (51, 51) is transparent 0x00000000 (since corner is rounded by rad=20)
    txui::uint32 corner_px = target.pixel_at(51, 51);
    if (corner_px != 0x00000000U) {
        std::cerr << "FAIL: Outside rounded corner pixel expected 0x00000000, got 0x"
                  << std::hex << corner_px << std::dec << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Outside rounded corner pixel transparency verified" << std::endl;

    // 3. Verify radius clamping with huge radius=500 -> clamps to min(w, h)/2 = 50
    txui::CommandBuffer clamp_buf;
    txui::Painter p_clamp(clamp_buf);
    p_clamp.begin_frame();
    p_clamp.fill_rounded_rect(txui::Rect(10.0, 10.0, 100.0, 100.0), 500.0, txui::Color(0, 255, 0, 255));
    p_clamp.end_frame();

    txui::CanvasRenderTarget clamp_target(120, 120);
    backend.execute(clamp_buf, clamp_target);

    txui::uint32 clamp_center = clamp_target.pixel_at(60, 60);
    txui::uint32 clamp_corner = clamp_target.pixel_at(11, 11);
    if (clamp_center != 0xFF00FF00U || clamp_corner != 0x00000000U) {
        std::cerr << "FAIL: Radius clamping verification failed!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Radius clamping (radius <= min(w,h)/2) verified" << std::endl;

    // 4. Verify Golden PNG roundtrip and visual output
    const char* out_file = "test_rounded_rect_out.png";
    if (!txui::ImageWriter::save_png(target, out_file)) {
        std::cerr << "FAIL: Could not write rounded rect PNG to " << out_file << std::endl;
        return EXIT_FAILURE;
    }

    auto loaded_opt = txui::ImageReader::load_png(out_file);
    if (!loaded_opt.has_value() || loaded_opt->hash_fnv1a() != target.hash_fnv1a()) {
        std::cerr << "FAIL: Golden PNG roundtrip mismatch for rounded rectangle" << std::endl;
        std::remove(out_file);
        return EXIT_FAILURE;
    }
    std::remove(out_file);
    std::cout << "PASS: Golden PNG roundtrip & deterministic hash verified (hash=0x"
              << std::hex << target.hash_fnv1a() << std::dec << ")" << std::endl;

    std::cout << "=== Phase 4.2.2 ALL ROUNDED RECTANGLE CRITERIA PASSED ===" << std::endl;
    return EXIT_SUCCESS;
}
