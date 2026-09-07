#include "AboutWidget.hpp"
#include <txui/render/Canvas.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/ImageWriter.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <iostream>

using namespace tinexus;
using namespace tinexus::about;

int main() {
    std::cout << "[Visual Test] Rendering About Tinexus window..." << std::endl;
    auto widget = txui::make_ref<AboutWidget>();

    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "About Tinexus",
        widget,
        []() {}, []() {}, []() {}, [](uint32_t) {}
    );

    const uint32_t W = 680, H = 420;
    txui::Constraints constraints(0, W, 0, H);
    chrome->measure(constraints);
    chrome->layout(txui::Rect(0, 0, W, H));

    txui::Canvas canvas(W, H);
    canvas.clear(txui::Color(24, 24, 28, 255));
    txui::PixmanBackend backend;

    // Render Overview
    {
        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        chrome->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas);
    }

    if (!txui::ImageWriter::save_png(canvas, "about_tinexus_overview.png")) {
        std::cerr << "FAIL: Failed to save about_tinexus_overview.png" << std::endl;
        return 1;
    }
    std::cout << "[Visual Test] Successfully saved about_tinexus_overview.png" << std::endl;

    // Switch to Displays tab
    txui::Event ev_click;
    ev_click.type = txui::EventType::PointerButtonPress;
    ev_click.pointer.button = txui::MouseButton::Left;
    // Tab 1 (Displays) is around x = 200, y = 80 inside chrome
    ev_click.pointer.x = 200.0;
    ev_click.pointer.y = 80.0;
    chrome->handle_event(ev_click);

    canvas.clear(txui::Color(24, 24, 28, 255));
    {
        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        chrome->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas);
    }
    txui::ImageWriter::save_png(canvas, "about_tinexus_displays.png");

    return 0;
}
