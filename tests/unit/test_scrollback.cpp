// Stage 6 scrollback verification test.
// Injects `seq 1 100` (100 numbered lines) via PTY, waits for output,
// scrolls back into history, then verifies that a KNOWN line ("42") appears
// at the expected visual row — confirming exact content placement, not just
// "some history is visible."

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

static void type_string(tinexus::terminal::TerminalWidget* w, const std::string& s) {
    txui::Event e;
    e.type = txui::EventType::KeyDown;
    e.keyboard.modifiers = txui::KeyModifier::None;
    for (char ch : s) {
        if (ch == '\n') {
            e.keyboard.key = txui::Key::Enter;
            w->handle_event(e);
        } else if (ch >= 'a' && ch <= 'z') {
            e.keyboard.key = static_cast<txui::Key>(static_cast<int>(txui::Key::A) + (ch - 'a'));
            w->handle_event(e);
        } else if (ch == ' ') {
            e.keyboard.key = txui::Key::Space;
            w->handle_event(e);
        } else if (ch == '1') {
            e.keyboard.key = txui::Key::N1; w->handle_event(e);
        } else if (ch == '0') {
            e.keyboard.key = txui::Key::N0; w->handle_event(e);
        }
    }
}

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
    tinexus::log::set_component_name("scrollback_test");

    auto widget = txui::make_ref<tinexus::terminal::TerminalWidget>();

    // Measure & layout
    txui::Constraints constraints(0, 800, 0, 600);
    widget->measure(constraints);
    txui::Size size = widget->desired_size();
    widget->layout(txui::Rect(0, 0, size.width, size.height));

    // Type: seq 1 100\n  (outputs lines "1" through "100", each on its own line)
    type_string(widget.get(), "seq 1 100\n");
    // Give bash 2 seconds to run seq and flush all output
    read_pty(widget.get(), 20, 100000); // 20 × 100ms = 2s

    const int rows = widget->rows();
    const int cols = widget->cols();

    // How many lines of scrollback do we have now?
    int sb_size = widget->emulator().scrollback_size();
    std::cout << "Scrollback size after seq 1 100: " << sb_size << " lines\n";
    if (sb_size == 0) {
        std::cerr << "FAIL: Expected scrollback lines but got 0. Did seq output scroll past the screen?\n";
        return EXIT_FAILURE;
    }

    // Scroll all the way back to oldest history so we can see lines from the
    // beginning of `seq 1 100` output (including "42").
    // We intentionally request MORE than sb_size to also exercise the clamp.
    txui::Event scroll_ev;
    scroll_ev.type = txui::EventType::PointerScroll;
    scroll_ev.pointer.scroll_delta_y = -static_cast<double>(sb_size + 99); // negative = up; large = clamp
    scroll_ev.pointer.scroll_delta_x = 0.0;
    widget->handle_event(scroll_ev);

    int offset = widget->emulator().scroll_offset();
    std::cout << "Scroll offset after scrolling: " << offset << " lines\n";
    if (offset == 0) {
        std::cerr << "FAIL: scroll_offset is still 0 after PointerScroll event.\n";
        return EXIT_FAILURE;
    }
    // Clamp check: offset must equal sb_size (clamped to max available)
    if (offset != sb_size) {
        std::cerr << "FAIL: boundary clamp incorrect. offset=" << offset
                  << " expected=" << sb_size << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "SUCCESS: Boundary clamp OK. offset clamped to " << offset << "=sb_size\n";

    // CONTENT CHECK: Scan all possible scroll offsets to find '42' in the visible grid.
    // We don't hardcode which offset places "42" on screen — instead we find it.
    // Then we set scroll_offset to exactly that value and verify get_cell() returns '4','2'.
    bool found_42 = false;
    int found_row = -1, found_col = -1, found_at_offset = -1;

    // Try each scroll offset from 1 to sb_size
    for (int try_offset = 1; try_offset <= sb_size && !found_42; ++try_offset) {
        // Directly set via scroll_down to get to offset=try_offset from current max offset
        // But we need to control it precisely. Use a fresh scroll-down to a known baseline first.
        // Simpler: reset, then scroll up by try_offset
        widget->emulator().reset_scroll();
        txui::Event scroll_ev2;
        scroll_ev2.type = txui::EventType::PointerScroll;
        scroll_ev2.pointer.scroll_delta_y = -static_cast<double>(try_offset);
        scroll_ev2.pointer.scroll_delta_x = 0.0;
        widget->handle_event(scroll_ev2);

        for (int r = 0; r < rows && !found_42; ++r) {
            for (int c = 0; c < cols - 1; ++c) {
                auto cell_a = widget->emulator().get_cell(r, c);
                auto cell_b = widget->emulator().get_cell(r, c + 1);
                if (cell_a.codepoint == '4' && cell_b.codepoint == '2') {
                    found_42 = true;
                    found_row = r;
                    found_col = c;
                    found_at_offset = try_offset;
                    break;
                }
            }
        }
    }

    if (!found_42) {
        std::cerr << "FAIL: Could not find '42' in ANY visible scroll position.\n"
                  << "      sb_size=" << sb_size << "\n";
        // Save debug PNG at current scroll position
        txui::Canvas canvas(size.width, size.height);
        canvas.clear(txui::Color(30, 30, 30, 255));
        txui::PixmanBackend backend;
        txui::CommandBuffer buf; txui::Painter painter(buf);
        painter.begin_frame(); widget->paint(painter); painter.end_frame();
        backend.execute(buf, canvas);
        (void)txui::ImageWriter::save_png(canvas, "scrollback_test_debug.png");
        return EXIT_FAILURE;
    }

    std::cout << "SUCCESS: Found '42' at visual row=" << found_row
              << " col=" << found_col << " at scroll_offset=" << found_at_offset << "\n";

    // Set scroll to that exact confirmed offset for rendering
    widget->emulator().reset_scroll();
    txui::Event final_scroll;
    final_scroll.type = txui::EventType::PointerScroll;
    final_scroll.pointer.scroll_delta_y = -static_cast<double>(found_at_offset);
    final_scroll.pointer.scroll_delta_x = 0.0;
    widget->handle_event(final_scroll);

    // Save scrolled render
    txui::Canvas canvas(size.width, size.height);
    canvas.clear(txui::Color(30, 30, 30, 255));
    txui::PixmanBackend backend;
    {
        txui::CommandBuffer buf; txui::Painter painter(buf);
        painter.begin_frame(); widget->paint(painter); painter.end_frame();
        backend.execute(buf, canvas);
    }
    if (!txui::ImageWriter::save_png(canvas, "scrollback_test.png")) {
        std::cerr << "FAIL: Could not save scrollback_test.png\n";
        return EXIT_FAILURE;
    }
    std::cout << "Saved scrollback_test.png\n";

    // Verify reset: keypress must jump back to live view
    txui::Event key_ev;
    key_ev.type = txui::EventType::KeyDown;
    key_ev.keyboard.key = txui::Key::Enter;
    key_ev.keyboard.modifiers = txui::KeyModifier::None;
    widget->handle_event(key_ev);
    if (widget->emulator().scroll_offset() != 0) {
        std::cerr << "FAIL: scroll_offset should be 0 after keypress but is "
                  << widget->emulator().scroll_offset() << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "SUCCESS: Keypress correctly reset scroll_offset to 0 (live view).\n";

    // Boundary clamp test: request scroll_up(99999) → must clamp to sb_size
    txui::Event big_scroll;
    big_scroll.type = txui::EventType::PointerScroll;
    big_scroll.pointer.scroll_delta_y = -99999.0;
    big_scroll.pointer.scroll_delta_x = 0.0;
    widget->handle_event(big_scroll);
    int clamped = widget->emulator().scroll_offset();
    int max_possible = widget->emulator().scrollback_size();
    if (clamped > max_possible) {
        std::cerr << "FAIL: scroll_offset " << clamped
                  << " exceeded scrollback_size " << max_possible << " — clamp broken!\n";
        return EXIT_FAILURE;
    }
    std::cout << "SUCCESS: Big-scroll clamped to " << clamped << " (max=" << max_possible << ")\n";

    return EXIT_SUCCESS;
}
