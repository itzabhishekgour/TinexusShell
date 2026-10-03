#include "StoreWidget.hpp"
#include "common/logger.hpp"
#include "common/version.hpp"
#include <txui/window/Window.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <txui/input/Event.hpp>
#include <txui/core/SingleInstance.hpp>

int main(int /*argc*/, char** /*argv*/) {
    tinexus::log::set_component_name("tinexus-store");
    tinexus::log::info("Starting Tinexus App Store v{}", tinexus::VERSION_STRING);

    txui::SingleInstance single_instance("tinexus-store");
    if (!single_instance.is_primary()) {
        single_instance.request_focus_primary();
        return 0;
    }

    auto window = txui::Window::create(960, 640, "App Store");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("[store] Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::store::StoreWidget>();

    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "App Store",
        root,
        [w = window.get()]() { w->on_close_request(); },
        [w = window.get()]() { w->minimize(); },
        [w = window.get()]() { w->set_maximized(!w->is_maximized()); },
        [w = window.get()](uint32_t serial) { w->start_interactive_move(serial); },
        [w = window.get()](uint32_t edges, uint32_t serial) { w->start_interactive_resize(edges, serial); }
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
