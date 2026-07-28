#include <txui/render/CanvasRenderTarget.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/ImageWriter.hpp>
#include <txui/core/Version.hpp>
#include <iostream>

int main() {
    std::cout << "=== txui-demo-02-rounded-rects (Phase 4.2.2 Vertical Slice) ===" << std::endl;
    std::cout << "libtxui Version: " << txui::TXUI_VERSION_STRING << std::endl;

    txui::CanvasRenderTarget canvas(600, 400);
    canvas.clear(txui::Color(25, 30, 40, 255)); // Deep dark blue background

    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);

    painter.begin_frame();

    // 1. Large glassmorphic panel with radius=32
    painter.fill_rounded_rect(
        txui::Rect(50.0, 40.0, 500.0, 320.0),
        32.0,
        txui::Color(255, 255, 255, 35) // Semi-transparent glass panel
    );

    // 2. Vibrant orange card inside panel with radius=20
    painter.fill_rounded_rect(
        txui::Rect(80.0, 80.0, 200.0, 240.0),
        20.0,
        txui::Color(240, 90, 40, 240)
    );

    // 3. Sleek cyan card with radius=16
    painter.fill_rounded_rect(
        txui::Rect(310.0, 80.0, 210.0, 110.0),
        16.0,
        txui::Color(30, 190, 230, 220)
    );

    // 4. Accent button with radius=12
    painter.fill_rounded_rect(
        txui::Rect(310.0, 210.0, 210.0, 110.0),
        12.0,
        txui::Color(140, 60, 220, 240)
    );

    painter.end_frame();

    std::cout << "Recorded " << buffer.size() << " commands." << std::endl;

    txui::PixmanBackend backend;
    backend.execute(buffer, canvas);

    const char* out_filename = "output_rounded.png";
    if (txui::ImageWriter::save_png(canvas, out_filename)) {
        std::cout << "SUCCESS: Wrote rendered rounded rectangles output to " << out_filename
                  << " (hash=0x" << std::hex << canvas.hash_fnv1a() << std::dec << ")" << std::endl;
    } else {
        std::cerr << "FAILED: Could not write PNG file " << out_filename << std::endl;
        return 1;
    }

    return 0;
}
