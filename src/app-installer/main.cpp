#include "InstallerWidget.hpp"
#include "common/logger.hpp"
#include "common/version.hpp"
#include <txui/window/Window.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <txui/input/Event.hpp>

int main(int /*argc*/, char** /*argv*/) {
    tinexus::log::set_component_name("tinexus-installer");
    tinexus::log::info("Starting Tinexus Application Installer v{}", tinexus::VERSION_STRING);

    auto window = txui::Window::create(800, 600, "Install Application");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("[installer] Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::app_installer::InstallerWidget>();

    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "Install Application",
        root,
        [w = window.get()]() { w->on_close_request(); },
        [w = window.get()]() { w->minimize(); },
        [w = window.get()]() { w->set_maximized(!w->is_maximized()); },
        [w = window.get()](uint32_t serial) { w->start_interactive_move(serial); }
    );
    window->set_root_widget(chrome);

    bool running = true;
    while (running && !window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            }
            chrome->handle_event(event);
        }

        window->present();
        window->wait();
    }

    tinexus::log::info("[installer] Exiting.");
    return 0;
}
