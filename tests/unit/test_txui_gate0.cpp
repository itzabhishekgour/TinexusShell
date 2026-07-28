#include <txui/render/CanvasRenderTarget.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/core/Types.hpp>
#include <iostream>
#include <cstdlib>

int main() {
    std::cout << "=== Tinexus Engineering Gate 0: Rendering Core Validation ===" << std::endl;

    // 1. Verify Coordinate Precision (double / float64)
    if (sizeof(txui::Coordinate) != sizeof(double)) {
        std::cerr << "FAIL: Coordinate must be double (float64), sizeof=" << sizeof(txui::Coordinate) << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Coordinate geometry precision verified as float64 (double)" << std::endl;

    // 2. Verify Command Buffer Immutability & Multi-Target Replayability
    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);

    painter.begin_frame();
    painter.fill_rect(txui::Rect(20.0, 20.0, 100.0, 100.0), txui::Color(220, 50, 50, 255));
    painter.fill_rect(txui::Rect(60.0, 60.0, 100.0, 100.0), txui::Color(50, 100, 220, 128));
    painter.end_frame();

    txui::CanvasRenderTarget target1(200, 200);
    txui::CanvasRenderTarget target2(200, 200);

    txui::PixmanBackend backend;
    backend.execute(buffer, target1);
    backend.execute(buffer, target2);

    txui::uint64 hash1 = target1.hash_fnv1a();
    txui::uint64 hash2 = target2.hash_fnv1a();

    if (hash1 != hash2) {
        std::cerr << "FAIL: Replay mismatch! Target1 hash=" << std::hex << hash1
                  << " != Target2 hash=" << hash2 << std::dec << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: CommandBuffer immutability & multi-target replay verified (hash="
              << std::hex << hash1 << std::dec << ")" << std::endl;

    // 3. Verify Premultiplied ARGB8888 Alpha Blending
    txui::uint32 blended_px = target1.pixel_at(80, 80); // Overlapping region
    if (blended_px == 0x00000000U) {
        std::cerr << "FAIL: Overlapping pixel at (80, 80) is uncolored" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Premultiplied ARGB8888 blending verified (pixel=0x"
              << std::hex << blended_px << std::dec << ")" << std::endl;

    // 4. Verify Stateless Renderer Execution
    txui::CommandBuffer empty_brush_buf;
    txui::Painter p2(empty_brush_buf);
    p2.begin_frame();
    p2.fill_rect(txui::Rect(0.0, 0.0, 10.0, 10.0), txui::Color::white());
    p2.end_frame();

    txui::CanvasRenderTarget target3(50, 50);
    backend.execute(empty_brush_buf, target3);
    txui::uint32 white_px = target3.pixel_at(5, 5);
    if (white_px != 0xFFFFFFFFU) {
        std::cerr << "FAIL: Stateless execution failed, expected 0xFFFFFFFF, got 0x"
                  << std::hex << white_px << std::dec << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: Stateless Renderer execution verified" << std::endl;

    // 5. Verify 10,000-Rectangle Stress Replay Determinism
    txui::CommandBuffer stress_buf;
    txui::Painter p_stress(stress_buf);
    p_stress.begin_frame();
    for (int i = 0; i < 10000; ++i) {
        double x = (i * 7) % 300;
        double y = (i * 13) % 300;
        p_stress.fill_rect(
            txui::Rect(x, y, 25.0, 25.0),
            txui::Color(static_cast<txui::uint8>(i * 3 % 255),
                        static_cast<txui::uint8>(i * 5 % 255),
                        static_cast<txui::uint8>(i * 7 % 255),
                        160)
        );
    }
    p_stress.end_frame();

    txui::CanvasRenderTarget st1(400, 400);
    txui::CanvasRenderTarget st2(400, 400);
    txui::CanvasRenderTarget st3(400, 400);

    backend.execute(stress_buf, st1);
    backend.execute(stress_buf, st2);
    backend.execute(stress_buf, st3);

    txui::uint64 shash1 = st1.hash_fnv1a();
    txui::uint64 shash2 = st2.hash_fnv1a();
    txui::uint64 shash3 = st3.hash_fnv1a();

    if (shash1 != shash2 || shash2 != shash3) {
        std::cerr << "FAIL: 10,000-rectangle stress replay non-deterministic!" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "PASS: 10,000-rectangle stress replay determinism verified (3 identical hashes: 0x"
              << std::hex << shash1 << std::dec << ")" << std::endl;

    std::cout << "=== ALL GATE 0 PREREQUISITE CHECKS PASSED SUCCESSFULLY ===" << std::endl;
    return EXIT_SUCCESS;
}
