#include "terminal/pty_process.hpp"
#include "terminal/TerminalEmulator.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

#include "TerminalWidget.hpp"
#include <txui/window/Window.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <txui/wayland/WaylandEventLoop.hpp>
#include <chrono>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("terminal");
    tinexus::log::info("Starting Tinexus Terminal (tinexus-terminal)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Terminal: Connected to Tinexus Platform IPC broker via SDK.");
    }

    // Create the main window
    auto window = txui::Window::create(800, 600, "Tinexus Terminal", false, "tinexus-terminal");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("Failed to create Wayland window.");
        sdk_client.disconnect();
        return 1;
    }

    // Create terminal widget
    auto terminal_widget = txui::make_ref<tinexus::terminal::TerminalWidget>();
    terminal_widget->set_window(window.get());
    // Create the Chrome/Window frame with traffic lights
    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "Tinexus Terminal",
        terminal_widget,
        [window = window.get()]() { window->close(); }, // on_close
        [window = window.get()]() { window->minimize(); }, // on_minimize
        [window = window.get()]() { window->set_maximized(!window->is_maximized()); }, // on_maximize
        [window = window.get()](uint32_t serial) { window->start_interactive_move(serial); }, // on_move
        [window = window.get()](uint32_t edges, uint32_t serial) { window->start_interactive_resize(edges, serial); } // on_resize
    );
    window->set_root_widget(chrome);

    // Wire up the PTY fd to the window's event loop
    if (window->event_loop()) {
        int master_fd = terminal_widget->pty().master_fd();
        window->event_loop()->add_fd(master_fd, [terminal_widget](int fd, uint32_t mask) {
            char buffer[4096];
            ssize_t n = terminal_widget->pty().read_bytes(buffer, sizeof(buffer));
            if (n > 0) {
                terminal_widget->emulator().write_input(buffer, n);
                terminal_widget->mark_needs_paint();
            }
        });
    }

    // Terminal event loop with blink timer
    auto last_blink_time = std::chrono::steady_clock::now();
    bool first_frame = true;
    
    while (!window->should_close()) {
        auto now = std::chrono::steady_clock::now();
        
        // Blink timer (500ms)
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_blink_time).count() >= 500) {
            terminal_widget->tick_blink();
            last_blink_time = now;
        }
        
        bool needs_redraw = first_frame || !terminal_widget->emulator().dirty_rects().empty();
        
        txui::Event event;
        while (window->poll_event(event)) {
            if (window->root_widget()) {
                window->root_widget()->handle_event(event);
            }
            needs_redraw = true;
        }
        
        if (needs_redraw) {
            window->present();
            first_frame = false;
        }
        
        window->wait_timeout(16);
    }

    tinexus::log::info("Terminal window closed.");
    sdk_client.disconnect();
    return 0;
}
