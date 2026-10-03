#include "settings/SettingsWidget.hpp"
#include "common/logger.hpp"
#include "common/version.hpp"
#include <txui/window/Window.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <txui/input/Event.hpp>
#include <txui/core/SingleInstance.hpp>

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-settings-ui");

    txui::SingleInstance single_instance("tinexus-settings");
    if (!single_instance.is_primary()) {
        single_instance.request_focus_primary();
        return 0;
    }

    tinexus::log::info("Starting Tinexus Control Center v{}", tinexus::VERSION_STRING);

    auto window = txui::Window::create(1000, 640, "Tinexus Settings", false, "tinexus-settings");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("[settings-ui] Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::settings_ui::SettingsWidget>();

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--page" && i + 1 < argc) {
            std::string page = argv[++i];
            if (page == "network" || page == "wifi") {
                root->select_page(tinexus::settings_ui::SettingsPage::Network);
            } else if (page == "display") {
                root->select_page(tinexus::settings_ui::SettingsPage::Display);
            } else if (page == "personalization") {
                root->select_page(tinexus::settings_ui::SettingsPage::Personalization);
            } else if (page == "system") {
                root->select_page(tinexus::settings_ui::SettingsPage::System);
            } else if (page == "shortcuts") {
                root->select_page(tinexus::settings_ui::SettingsPage::KeyboardShortcuts);
            } else if (page == "privacy") {
                root->select_page(tinexus::settings_ui::SettingsPage::PrivacySecurity);
            } else if (page == "about") {
                root->select_page(tinexus::settings_ui::SettingsPage::About);
            }
        } else if (arg == "-n" || arg == "--network" || arg == "--wifi") {
            root->select_page(tinexus::settings_ui::SettingsPage::Network);
        }
    }

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
        },
        // ── Resize: initiate interactive resize via xdg_toplevel.resize ───────
        [w = window.get()](uint32_t edges, uint32_t serial) {
            w->start_interactive_resize(edges, serial);
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
