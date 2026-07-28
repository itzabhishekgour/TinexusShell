#include <txui/render/Canvas.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/ImageWriter.hpp>
#include <txui/core/Version.hpp>
#include <iostream>

int main() {
    std::cout << "=== txui-demo-01-canvas (Phase 4.2.1 Vertical Slice) ===" << std::endl;
    std::cout << "libtxui Version: " << txui::TXUI_VERSION_STRING << std::endl;

    txui::Canvas canvas(400, 400);
    canvas.clear(txui::Color(245, 245, 250, 255)); // Light gray background

    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);

    painter.begin_frame();

    // Solid opaque red rectangle
    painter.fill_rect(txui::Rect(50.0, 50.0, 200.0, 150.0), txui::Color(220, 40, 40, 255));

    // Overlapping translucent blue rectangle (tests alpha blending)
    painter.fill_rect(txui::Rect(150.0, 100.0, 200.0, 150.0), txui::Color(40, 80, 220, 160));

    // Overlapping green rectangle
    painter.fill_rect(txui::Rect(100.0, 200.0, 120.0, 120.0), txui::Color(40, 180, 60, 200));

    painter.end_frame();

    std::cout << "Recorded " << buffer.size() << " commands." << std::endl;

    txui::PixmanBackend backend;
    backend.execute(buffer, canvas);

    const char* out_filename = "output.png";
    if (txui::ImageWriter::save_png(canvas, out_filename)) {
        std::cout << "SUCCESS: Wrote rendered output to " << out_filename << std::endl;
    } else {
        std::cerr << "FAILED: Could not write PNG file " << out_filename << std::endl;
        return 1;
    }

    return 0;
}
