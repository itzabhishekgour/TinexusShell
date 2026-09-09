#include "monitor/MonitorWidget.hpp"
#include <txui/window/Window.hpp>
#include <txui/widgets/ChromeWidget.hpp>
#include <txui/input/Event.hpp>
#include <txui/core/SingleInstance.hpp>
#include <common/logger.hpp>
#include <common/version.hpp>
#include <chrono>

int main(int /*argc*/, char** /*argv*/) {
    tinexus::log::set_component_name("tinexus-monitor");

    txui::SingleInstance single_instance("tinexus-monitor");
    if (!single_instance.is_primary()) {
        single_instance.request_focus_primary();
        return 0;
    }

    tinexus::log::info("Starting Tinexus Activity Monitor v{}", tinexus::VERSION_STRING);

    // 860 x 580 window — premium Activity Monitor proportions
    auto window = txui::Window::create(860, 580, "Activity Monitor", false, "tinexus-monitor");
    if (!window || !window->is_wayland_connected()) {
        tinexus::log::error("[monitor] Failed to connect to Wayland display!");
        return 1;
    }

    auto root = txui::make_ref<tinexus::monitor::MonitorWidget>();

    // Wrap in ChromeWidget for native traffic lights (🔴 🟡 🟢) and titlebar
    auto chrome = txui::make_ref<txui::ChromeWidget>(
        "Activity Monitor",
        root,
        [w = window.get()]() { w->on_close_request(); },
        [w = window.get()]() { w->minimize(); },
        [w = window.get()]() { w->set_maximized(!w->is_maximized()); },
        [w = window.get()](uint32_t serial) { w->start_interactive_move(serial); }
    );
    window->set_root_widget(chrome);

    auto last_telemetry_time = std::chrono::steady_clock::now();
    bool running = true;

    while (running && !window->should_close()) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            }
            chrome->handle_event(event);
        }

        // Live refresh every 1.5 seconds
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_telemetry_time).count() >= 1500) {
            root->refresh_telemetry();
            window->request_repaint();
            last_telemetry_time = now;
        }

        window->present();
        window->wait_timeout(100);
    }

    tinexus::log::info("[monitor] Exiting cleanly.");
    return 0;
}
