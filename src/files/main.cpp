#include "files/column_view_model.hpp"
#include "files/trash_manager.hpp"
#include "files/file_operations.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <csignal>
#include "ui/FilesWindow.hpp"
#include "tinexus_protocols_client.h"

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("files");
    tinexus::log::info("Starting Tinexus Files...");
    signal(SIGCHLD, SIG_IGN);

    // Initialize SDK client for platform services
    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Files: Connected to Tinexus Platform IPC broker.");
    }

    std::filesystem::path target_path = (argc > 1) ? std::filesystem::path(argv[1]) : std::filesystem::current_path();

    auto window = txui::Window::create(1000, 600, "tinexus-files");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("Failed to connect to Wayland display.");
        return 1;
    }

    auto files_window = std::make_shared<tinexus::files::ui::ColumnBrowserWidget>();
    files_window->set_on_execute([&](const std::filesystem::path& path) {
        // Launch via IPC (Phase 1)
        if (sdk_client.is_connected()) {
            tinexus::log::info("Launching {} via SDK", path.string());
            // SDK Launch not implemented yet, just log for now
        }
    });

    window->set_root_widget(files_window);
    files_window->navigate_to(target_path);

    window->present();

    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        bool needs_redraw = false;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else {
                if (files_window->handle_event(event)) {
                    needs_redraw = true;
                }
            }
        }
        
        if (files_window->needs_paint() || files_window->needs_layout()) {
            needs_redraw = true;
        }

        if (needs_redraw) {
            window->present();
        }

        window->wait_timeout(16); // 60fps max polling
    }

    sdk_client.disconnect();
    tinexus::log::info("Tinexus Files exiting cleanly.");
    return 0;
}
