#include "settings/SettingsWidget.hpp"
#include "common/logger.hpp"
#include "common/version.hpp"
#include <txui/window/Window.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <txui/input/Event.hpp>

int main(int /*argc*/, char** /*argv*/) {
    tinexus::log::set_component_name("tinexus-settings-ui");
    tinexus::log::info("Starting Tinexus Control Center v{}", tinexus::VERSION_STRING);

    auto window = txui::Window::create(1000, 640, "Tinexus Settings");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("[settings-ui] Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::settings_ui::SettingsWidget>();

    // Wrap in ChromeWidget — provides macOS-style window chrome with
    // fully functional close, minimize, and maximize buttons.
    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "Tinexus Settings",
        root,
        // ── Close: request application exit ──────────────────────────────────
        [w = window.get()]() {
            w->on_close_request();
        },
        // ── Minimize: hide window via xdg_toplevel.minimize request ──────────
        // Compositor will hide the scene node. No restore until dock exists.
        [w = window.get()]() {
            w->minimize();
        },
        // ── Maximize: toggle maximize/restore ─────────────────────────────────
        [w = window.get()]() {
            w->set_maximized(!w->is_maximized());
        },
        // ── Move: initiate interactive drag via xdg_toplevel.move ─────────────
        [w = window.get()](uint32_t serial) {
            w->start_interactive_move(serial);
        }
    );
    window->set_root_widget(chrome);

    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            }
            // Pass input events to the UI tree (ChromeWidget → SettingsWidget)
            chrome->handle_event(event);
        }

        window->present();
        window->wait();
    }

    tinexus::log::info("[settings-ui] Exiting.");
    return 0;
}
