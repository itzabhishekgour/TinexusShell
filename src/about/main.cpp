#include "AboutWidget.hpp"
#include <txui/window/Window.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <txui/input/Event.hpp>
#include <common/logger.hpp>
#include <common/version.hpp>

int main(int /*argc*/, char** /*argv*/) {
    tinexus::log::set_component_name("tinexus-about");
    tinexus::log::info("Starting About Tinexus Profiler v{}", tinexus::VERSION_STRING);

    // 680 x 420 window — authentic macOS About window proportions
    auto window = txui::Window::create(680, 420, "About Tinexus");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("[about] Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::about::AboutWidget>();

    // Wrap in ChromeWidget for traffic lights (🔴 🟡 🟢) and window move
    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "About Tinexus",
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
        window->wait_timeout(100);
    }

    tinexus::log::info("[about] Exiting cleanly.");
    return 0;
}
