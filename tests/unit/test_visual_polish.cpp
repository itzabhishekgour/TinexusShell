// Stage 7 visual polish verification test.
// Verifies:
// 1. Cursor blink only dirties a small rect (1 cell).
// 2. Selection highlight (live view).
// 3. Rounded frame visually.

#include <txui/render/Canvas.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/ImageWriter.hpp>
#include <iostream>
#include <string>
#include <cstdlib>
#include <unistd.h>
#include "../../src/terminal/TerminalWidget.hpp"
#include "common/logger.hpp"

static void read_pty(tinexus::terminal::TerminalWidget* w, int polls, int delay_us) {
    char buf[4096];
    for (int i = 0; i < polls; ++i) {
        ssize_t n = w->pty().read_bytes(buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            w->emulator().write_input(buf, static_cast<size_t>(n));
        }
        usleep(delay_us);
    }
}

int main() {
    tinexus::log::set_component_name("visual_polish_test");
    auto widget = txui::make_ref<tinexus::terminal::TerminalWidget>();

    // Measure & layout
    txui::Constraints constraints(0, 800, 0, 600);
    widget->measure(constraints);
    txui::Size size = widget->desired_size();
    widget->layout(txui::Rect(0, 0, size.width, size.height));

    // Send some text to have a background
    txui::Event e; e.type = txui::EventType::KeyDown; e.keyboard.modifiers = txui::KeyModifier::None;
    e.keyboard.key = txui::Key::L; widget->handle_event(e);
    e.keyboard.key = txui::Key::S; widget->handle_event(e);
    e.keyboard.key = txui::Key::Space; widget->handle_event(e);
    e.keyboard.key = txui::Key::Minus; widget->handle_event(e);
    e.keyboard.key = txui::Key::L; widget->handle_event(e);
    e.keyboard.key = txui::Key::Enter; widget->handle_event(e);

    read_pty(widget.get(), 15, 100000); // Wait for ls -l output

    // Frame 1: Base render
    txui::Canvas canvas(size.width, size.height);
    canvas.clear(txui::Color(30, 30, 30, 255));
    txui::PixmanBackend backend;
    
    // Clear dirty rects before first paint to be sure
    widget->emulator().clear_dirty_rects();
    widget->emulator().mark_all_dirty();
    
    {
        txui::CommandBuffer buf; txui::Painter painter(buf);
        painter.begin_frame(); widget->paint(painter); painter.end_frame();
        backend.execute(buf, canvas);
    }

    // Now trigger a blink
    widget->tick_blink();

    // Check dirty rects size — should be exactly 1 cell
    auto rects = widget->emulator().dirty_rects();
    if (rects.empty()) {
        std::cerr << "FAIL: tick_blink() did not produce any dirty rects!\n";
        return EXIT_FAILURE;
    }
    
    // Total cells dirtied
    int dirtied_cells = 0;
    for (const auto& r : rects) {
        dirtied_cells += (r.end_row - r.start_row) * (r.end_col - r.start_col);
    }
    std::cout << "Cells dirtied by blink: " << dirtied_cells << "\n";
    if (dirtied_cells != 1) {
        std::cerr << "FAIL: Blink dirtied " << dirtied_cells << " cells instead of 1. Full repaint triggered?\n";
        return EXIT_FAILURE;
    }
    std::cout << "SUCCESS: Cursor blink only dirtied 1 cell.\n";

    // Create a new canvas to see the blink change via dirty rects
    txui::Canvas canvas_blink(size.width, size.height);
    std::copy(canvas.pixels().begin(), canvas.pixels().end(), canvas_blink.pixels().begin());

    {
        txui::CommandBuffer buf; txui::Painter painter(buf);
        painter.begin_frame(); widget->paint(painter); painter.end_frame(); // consumes dirty rects
        backend.execute(buf, canvas_blink);
    }

    if (!txui::ImageWriter::save_png(canvas, "polish_pre_blink.png") ||
        !txui::ImageWriter::save_png(canvas_blink, "polish_post_blink.png")) {
        std::cerr << "FAIL: save_png failed\n";
        return EXIT_FAILURE;
    }

    // Now let's trigger a selection and render it
    txui::Event sel_start;
    sel_start.type = txui::EventType::PointerButtonPress;
    sel_start.pointer.x = 20.0;
    sel_start.pointer.y = 20.0; // row 1, col 2 roughly
    widget->handle_event(sel_start);
    
    txui::Event sel_move;
    sel_move.type = txui::EventType::PointerMove;
    sel_move.pointer.x = 100.0;
    sel_move.pointer.y = 60.0; // multi-row selection
    widget->handle_event(sel_move);
    
    txui::Event sel_end;
    sel_end.type = txui::EventType::PointerButtonRelease;
    widget->handle_event(sel_end);

    txui::Canvas canvas_sel(size.width, size.height);
    std::copy(canvas_blink.pixels().begin(), canvas_blink.pixels().end(), canvas_sel.pixels().begin());
    
    {
        txui::CommandBuffer buf; txui::Painter painter(buf);
        painter.begin_frame(); widget->paint(painter); painter.end_frame();
        backend.execute(buf, canvas_sel);
    }
    if (!txui::ImageWriter::save_png(canvas_sel, "polish_selection.png")) {
        return EXIT_FAILURE;
    }

    std::cout << "SUCCESS: Generated visual polish screenshots.\n";
    return EXIT_SUCCESS;
}
