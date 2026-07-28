#include <txui/render/Canvas.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <cstdlib>

int main() {
    txui::Canvas canvas(200, 200);
    canvas.clear(txui::Color::transparent());

    txui::CommandBuffer buffer;
    txui::Painter painter(buffer);

    painter.begin_frame();
    painter.fill_rect(txui::Rect(50.0, 50.0, 100.0, 100.0), txui::Color(255, 0, 0, 255));
    painter.end_frame();

    txui::PixmanBackend backend;
    backend.execute(buffer, canvas);

    txui::uint32 center_px = canvas.pixel_at(100, 100);
    if (center_px != 0xFFFF0000U) {
        std::cerr << "FAIL: Center pixel (100, 100) expected 0xFFFF0000, got 0x"
                  << std::hex << center_px << std::dec << std::endl;
        return EXIT_FAILURE;
    }

    txui::uint32 corner_px = canvas.pixel_at(0, 0);
    if (corner_px != 0x00000000U) {
        std::cerr << "FAIL: Corner pixel (0, 0) expected 0x00000000, got 0x"
                  << std::hex << corner_px << std::dec << std::endl;
        return EXIT_FAILURE;
    }

    if (!txui::ImageWriter::save_png(canvas, "test_rect_out.png")) {
        std::cerr << "FAIL: ImageWriter::save_png returned false" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "PASS: test_txui_render pixel correctness verified successfully" << std::endl;
    return EXIT_SUCCESS;
}
