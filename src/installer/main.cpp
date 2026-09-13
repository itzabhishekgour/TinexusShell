#include "installer/InstallerWindow.hpp"
#include "installer/install_controller.hpp"
#include "installer/disk_inspector.hpp"
#include <txui/wayland/WaylandEventLoop.hpp>
#include "tinexus/client.hpp"
#include "common/logger.hpp"

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("tinexus-installer");
    tinexus::log::info("Starting Tinexus OS Installer (tinexus-installer)...");

    // Headless / Automated CLI installation mode:
    if (argc >= 3 && std::string(argv[1]) == "--target") {
        std::string target_dev = argv[2];
        bool dry_run = (argc >= 4 && std::string(argv[3]) == "--dry-run");
        tinexus::log::info("Running automated headless install targeting '{}' (dry_run: {})...", target_dev, dry_run);

        tinexus::installer::InstallController controller;
        tinexus::installer::DiskInfo disk;
        disk.device_path = target_dev;
        disk.model = "Target Installation Drive";
        disk.size_bytes = 15ULL * 1024 * 1024 * 1024;
        disk.is_live_media = false;

        bool ok = controller.run_installation(disk, dry_run);
        tinexus::log::info("Headless installation finished with result: {}", ok ? "SUCCESS" : "FAILURE");
        return ok ? 0 : 1;
    }

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
