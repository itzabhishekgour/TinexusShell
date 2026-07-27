#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "terminal/pty_process.hpp"
#include "terminal/ansi_parser.hpp"
#include "terminal/terminal_buffer.hpp"
#include "terminal/terminal_renderer.hpp"

void test_ansi_parser() {
    tinexus::terminal::AnsiParser parser;
    auto result = parser.parse("\033[1mHello World\033[0m");
    assert(result.size() == 11);
    assert(result[0].style.bold == true);
    std::cout << "[PASS] test_ansi_parser\n";
}

void test_terminal_buffer_scrollback() {
    tinexus::terminal::TerminalBuffer buffer(100);
    for (int i = 0; i < 150; ++i) {
        buffer.append_string("Line " + std::to_string(i) + "\n");
    }
    assert(buffer.line_count() <= 100);

    tinexus::terminal::TerminalRenderer renderer;
    auto rendered = renderer.render_plain_text(buffer);
    assert(!rendered.empty());
    std::cout << "[PASS] test_terminal_buffer_scrollback\n";
}

void test_pty_process_spawn_and_resize() {
    tinexus::terminal::PtyProcess pty;
    assert(pty.spawn("/bin/echo"));
    assert(pty.set_window_size(120, 40));

    char buf[128];
    ssize_t read_bytes = pty.read_bytes(buf, sizeof(buf));
    (void)read_bytes;

    std::cout << "[PASS] test_pty_process_spawn_and_resize\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_terminal");
    tinexus::log::info("Running Integration Test Suite for Tinexus Terminal...");

    test_ansi_parser();
    test_terminal_buffer_scrollback();
    test_pty_process_spawn_and_resize();

    tinexus::log::info("All Tinexus Terminal integration tests passed 100%!");
    return 0;
}
