#include <txui/render/Canvas.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include "../../src/terminal/TerminalWidget.hpp"
#include "common/logger.hpp"

int main() {
    tinexus::log::set_component_name("terminal_test");
    
    // Create widget
    auto widget = txui::make_ref<tinexus::terminal::TerminalWidget>();
    
    // Inject "ls --color\n"
    txui::Event e;
    e.type = txui::EventType::KeyDown;
    e.keyboard.modifiers = txui::KeyModifier::None;
    
    e.keyboard.key = txui::Key::L; widget->handle_event(e);
    e.keyboard.key = txui::Key::S; widget->handle_event(e);
    e.keyboard.key = txui::Key::Space; widget->handle_event(e);
    e.keyboard.key = txui::Key::Minus; widget->handle_event(e);
    e.keyboard.key = txui::Key::Minus; widget->handle_event(e);
    e.keyboard.key = txui::Key::C; widget->handle_event(e);
    e.keyboard.key = txui::Key::O; widget->handle_event(e);
    e.keyboard.key = txui::Key::L; widget->handle_event(e);
    e.keyboard.key = txui::Key::O; widget->handle_event(e);
    e.keyboard.key = txui::Key::R; widget->handle_event(e);
    e.keyboard.key = txui::Key::Enter; widget->handle_event(e);

    // Read output for a short while to let `ls --color` finish
    char buffer[4096];
    for (int i = 0; i < 10; ++i) { // Poll for 1 second
        ssize_t n = widget->pty().read_bytes(buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            widget->emulator().write_input(buffer, n);
        }
        usleep(100000); // 100ms
    }
    
    // Measure & layout
    txui::Constraints constraints(0, 800, 0, 600);
    widget->measure(constraints);
    txui::Size size = widget->desired_size();
    widget->layout(txui::Rect(0, 0, size.width, size.height));
    
    // Frame 1
    txui::Canvas canvas(size.width, size.height);
    canvas.clear(txui::Color(30, 30, 30, 255));

    txui::PixmanBackend backend;
    {
        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas);
    }
    
    // Send more events to cause damage (scrolling or just more text)
    txui::Event e2; e2.type = txui::EventType::KeyDown; e2.keyboard.modifiers = txui::KeyModifier::None;
    e2.keyboard.key = txui::Key::L; widget->handle_event(e2);
    e2.keyboard.key = txui::Key::S; widget->handle_event(e2);
    e2.keyboard.key = txui::Key::Enter; widget->handle_event(e2);
    
    for (int i = 0; i < 5; ++i) { // Poll for 0.5s
        ssize_t n = widget->pty().read_bytes(buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            widget->emulator().write_input(buffer, n);
        }
        usleep(100000); // 100ms
    }

    // Now emulator has dirty rects for the new damage.
    // Let's create two canvases from the current canvas state:
    txui::Canvas canvas_dirty(size.width, size.height);
    std::copy(canvas.pixels().begin(), canvas.pixels().end(), canvas_dirty.pixels().begin());

    txui::Canvas canvas_full(size.width, size.height);
    std::copy(canvas.pixels().begin(), canvas.pixels().end(), canvas_full.pixels().begin());

    // 1. DIRTY RECT MODE
    // We execute paint() which will consume the dirty rects and draw to canvas_dirty
    {
        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter); // Consumes dirty rects!
        painter.end_frame();
        backend.execute(buffer_cmds, canvas_dirty);
    }

    // 2. FULL REPAINT MODE
    // We force all dirty and execute paint() which will draw everything to canvas_full
    canvas_full.clear(txui::Color(30, 30, 30, 255));
    widget->emulator().mark_all_dirty();
    {
        txui::CommandBuffer buffer_cmds;
        txui::Painter painter(buffer_cmds);
        painter.begin_frame();
        widget->paint(painter);
        painter.end_frame();
        backend.execute(buffer_cmds, canvas_full);
    }

    if (!txui::ImageWriter::save_png(canvas_dirty, "terminal_render_dirty.png")) {
        std::cerr << "FAIL: ImageWriter::save_png returned false" << std::endl;
        return EXIT_FAILURE;
    }
    if (!txui::ImageWriter::save_png(canvas_full, "terminal_render_full.png")) {
        std::cerr << "FAIL: ImageWriter::save_png returned false" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Saved terminal_render_dirty.png and terminal_render_full.png successfully!" << std::endl;
    return EXIT_SUCCESS;
}
