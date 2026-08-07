#include "installer/InstallerWindow.hpp"
#include <txui/wayland/WaylandEventLoop.hpp>
#include "tinexus/client.hpp"
#include "common/logger.hpp"

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("tinexus-installer");
    tinexus::log::info("Starting Tinexus Graphical OS Installer (tinexus-installer)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Installer: Connected to Tinexus Platform IPC broker via SDK.");
    }

    // Launch the Graphical Installer
    auto window = txui::Window::create(800, 600, "Tinexus OS Installer");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::installer::InstallerWidget>();
    window->set_root_widget(root);

    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            }
        }
        window->present();
        window->wait();
    }

    sdk_client.disconnect();
    return 0;
}
