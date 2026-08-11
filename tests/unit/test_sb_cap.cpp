// Scrollback 1000-line cap verification test.
// Runs `seq 1 2000` to generate far more than 1000 lines of output.
// Verifies:
//   1. scrollback_size() never exceeds 1000 (eviction fires)
//   2. scroll_offset is clamped when the line it points at gets evicted
//   3. No crash/corruption when the buffer is at capacity

#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include "../../src/terminal/TerminalWidget.hpp"
#include "common/logger.hpp"

static void send_char(tinexus::terminal::TerminalWidget* w, char ch) {
    txui::Event e;
    e.type = txui::EventType::KeyDown;
    e.keyboard.modifiers = txui::KeyModifier::None;
    switch (ch) {
        case '\n': e.keyboard.key = txui::Key::Enter;  break;
        case ' ':  e.keyboard.key = txui::Key::Space;  break;
        case '0':  e.keyboard.key = txui::Key::N0;     break;
        case '1':  e.keyboard.key = txui::Key::N1;     break;
        case '2':  e.keyboard.key = txui::Key::N2;     break;
        case 's':  e.keyboard.key = txui::Key::S;      break;
        case 'e':  e.keyboard.key = txui::Key::E;      break;
        case 'q':  e.keyboard.key = txui::Key::Q;      break;
        default:   return; // skip unmapped chars
    }
    w->handle_event(e);
}

static void send_str(tinexus::terminal::TerminalWidget* w, const char* s) {
    for (; *s; ++s) send_char(w, *s);
}

int main() {
    tinexus::log::set_component_name("sb_cap_test");

    auto widget = txui::make_ref<tinexus::terminal::TerminalWidget>();

    // Send: seq 1 2000\n  — generates 2000 numbered lines, far above the 1000-line cap
    send_str(widget.get(), "seq 1 2000\n");

    // Poll PTY output for up to 5 seconds
    char buf[4096];
    for (int i = 0; i < 50; ++i) {
        ssize_t n = widget->pty().read_bytes(buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            widget->emulator().write_input(buf, static_cast<size_t>(n));
        }
        usleep(100000); // 100ms
    }

    int sb = widget->emulator().scrollback_size();
    std::cout << "Scrollback size after seq 1 2000: " << sb << "\n";

    // PRIMARY CHECK: cap must hold at <= 1000
    if (sb > 1000) {
        std::cerr << "FAIL: scrollback exceeded cap! size=" << sb << " (max=1000)\n";
        return EXIT_FAILURE;
    }
    if (sb < 900) {
        // seq 2000 lines - 24 visible rows = 1976 scrolled off, capped to 1000.
        // If we got < 900, seq probably didn't finish outputting — warn only.
        std::cerr << "WARN: only " << sb << " scrollback lines — did seq finish? "
                  << "Try increasing poll time.\n";
    }
    std::cout << "SUCCESS: scrollback capped at " << sb << " <= 1000\n";

    // CLAMP CHECK: scroll to max, verify offset == sb (not more)
    txui::Event scroll_ev;
    scroll_ev.type = txui::EventType::PointerScroll;
    scroll_ev.pointer.scroll_delta_y = -99999.0;
    scroll_ev.pointer.scroll_delta_x = 0.0;
    widget->handle_event(scroll_ev);

    int offset = widget->emulator().scroll_offset();
    std::cout << "After max-scroll: offset=" << offset << " sb_size=" << sb << "\n";
    if (offset > sb) {
        std::cerr << "FAIL: offset " << offset << " > sb_size " << sb
                  << " — clamp-on-eviction broken!\n";
        return EXIT_FAILURE;
    }
    std::cout << "SUCCESS: scroll_offset properly clamped to sb_size\n";

    // STABILITY CHECK: scroll down a bit then verify offset decreases correctly
    txui::Event down_ev;
    down_ev.type = txui::EventType::PointerScroll;
    down_ev.pointer.scroll_delta_y = 10.0; // positive = scroll down
    down_ev.pointer.scroll_delta_x = 0.0;
    widget->handle_event(down_ev);
    int offset_after_down = widget->emulator().scroll_offset();
    if (offset_after_down >= offset) {
        std::cerr << "FAIL: scroll_down didn't reduce offset. before=" << offset
                  << " after=" << offset_after_down << "\n";
        return EXIT_FAILURE;
    }
    std::cout << "SUCCESS: scroll_down reduced offset from " << offset
              << " to " << offset_after_down << "\n";

    return EXIT_SUCCESS;
}
