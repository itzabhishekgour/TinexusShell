#include "settings/SettingsWidget.hpp"
#include "common/logger.hpp"
#include "common/version.hpp"
#include <txui/window/Window.hpp>
#include <txui/core/Ref.hpp>
#include <txui/input/Event.hpp>

int main(int /*argc*/, char** /*argv*/) {
    tinexus::log::set_component_name("tinexus-settings-ui");
    tinexus::log::info("Starting Tinexus Control Center v{}", tinexus::VERSION_STRING);

    auto window = txui::Window::create(1280, 800, "Tinexus Settings");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("[settings-ui] Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::settings_ui::SettingsWidget>();
    window->set_root_widget(root);

    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            }
            // TODO: forward mouse clicks to sidebar for page switching
        }

        window->present();
        window->wait();
    }

    tinexus::log::info("[settings-ui] Exiting.");
    return 0;
}
