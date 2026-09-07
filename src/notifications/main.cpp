#include "notifications/notification_server.hpp"
#include "notifications/notification_manager.hpp"
#include "notifications/NotificationStackWidget.hpp"
#include "notifications/dbus_server.hpp"

#include <txui/window/Window.hpp>
#include <common/logger.hpp>

#include <csignal>
#include <atomic>
#include <chrono>

using namespace tinexus;

static std::atomic<bool> g_running{true};

static void signal_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    log::set_component_name("notifications");
    log::info("[Notifications] Tinexus Notification Center Daemon starting...");

    if (!notifications::DBusServer::instance().start()) {
        log::error("[Notifications] Failed to start DBus Server!");
    }

    // 1. Create TxUI Layer-Shell Window anchored to Top-Right
    auto window = txui::Window::create(420, 100, "tinexus-notifications", true /* layer_shell */);
    if (!window) {
        log::error("[Notifications] Failed to create TxUI layer-shell window!");
        return 1;
    }

    window->set_layer_shell_config(
        txui::LayerType::Overlay,
        txui::LayerAnchor::Top | txui::LayerAnchor::Right,
        -1 // No exclusive zone (overlays atop normal windows)
    );
    window->set_layer_margins(16, 16, 0, 0); // 16px from top-right display edge
    window->set_keyboard_interactivity(false);

    // 2. Set root widget to NotificationStackWidget
    auto stack_widget = txui::make_ref<notifications::NotificationStackWidget>();
    window->set_root_widget(stack_widget);

    stack_widget->set_on_height_changed([window_ptr = window.get()](uint32_t new_h) {
        window_ptr->resize(420, std::max(10u, new_h));
    });

    // 3. Register D-Bus socket with TxUI WaylandEventLoop
    if (notifications::DBusServer::instance().bus() && window->event_loop()) {
        int bus_fd = sd_bus_get_fd(notifications::DBusServer::instance().bus());
        if (bus_fd >= 0) {
            window->event_loop()->add_fd(bus_fd, [](int, uint32_t) {
                while (sd_bus_process(notifications::DBusServer::instance().bus(), nullptr) > 0) {}
            });
            log::info("[Notifications] Registered D-Bus event fd with TxUI event loop");
        }
    }

    // 4. Enqueue initial welcome notification
    notifications::NotificationServer::instance().notify(
        "Tinexus System",
        0,
        "tinexus",
        "Welcome to Tinexus OS",
        "Press Ctrl+K to open Pulse launcher",
        {},
        notifications::Urgency::Normal,
        8000
    );

    // 5. Main TxUI event and frame loop
    auto last_time = std::chrono::steady_clock::now();

    while (!window->should_close() && g_running) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                window->close();
            } else {
                stack_widget->handle_event(event);
            }
        }

        auto now = std::chrono::steady_clock::now();
        auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
        last_time = now;

        stack_widget->update(delta_ms);
        window->present();

        // 60 FPS when animations/bubbles are active, 100ms idle poll when empty
        window->wait_timeout(stack_widget->has_active_bubbles() ? 16 : 100);
    }

    log::info("[Notifications] Daemon shut down cleanly.");
    return 0;
}
