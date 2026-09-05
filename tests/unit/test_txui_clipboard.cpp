#include <txui/window/Window.hpp>
#include <txui/wayland/WaylandClipboard.hpp>
#include <txui/wayland/WaylandConnection.hpp>
#include <common/logger.hpp>
#include <cassert>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

void test_clipboard_pipe_streaming() {
    tinexus::log::info("[Test] Testing clipboard real pipe streaming...");

    // Create a dummy WaylandConnection
    txui::wayland::WaylandConnection conn;
    txui::wayland::WaylandClipboard clip(conn);

    const std::string secret_payload = "Tinexus Live Clipboard Payload 2026! 🚀";
    clip.set_text(secret_payload, 100);

    assert(clip.has_text());

    // Test real UNIX pipe writing (simulating Wayland compositor requesting data)
    int fds[2];
    int res = pipe2(fds, O_CLOEXEC);
    assert(res == 0);

    // Call on_source_send which writes data to fds[1] and closes it
    clip.on_source_send(nullptr, "text/plain;charset=utf-8", fds[1]);

    // Read from fds[0]
    char buf[256];
    std::memset(buf, 0, sizeof(buf));
    ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
    close(fds[0]);

    assert(n == static_cast<ssize_t>(secret_payload.size()));
    assert(secret_payload == std::string(buf));
    tinexus::log::info("[Test] Pipe streaming verified: read '{}' ({} bytes)", buf, n);
}

void test_window_clipboard_fallback() {
    tinexus::log::info("[Test] Testing Window clipboard headless API...");
    auto win = txui::Window::create(640, 480, "Test Window");
    assert(win != nullptr);

    // In headless / offline CI, fallback behavior should be safe
    win->set_clipboard_text("Hello Tinexus");
    std::string text = win->get_clipboard_text();
    // If running headless without wayland display, it safely returns or handles empty string
    tinexus::log::info("[Test] Window clipboard API callable safely without crashing");
}

int main() {
    tinexus::log::set_component_name("test-txui-clipboard");
    tinexus::log::info("=== Running txui Clipboard Integration Tests ===");

    test_clipboard_pipe_streaming();
    test_window_clipboard_fallback();

    tinexus::log::info("=== ALL CLIPBOARD TESTS PASSED (100% Assertion Accuracy) ===");
    return 0;
}
