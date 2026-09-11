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
        window_ptr->resize(420, std::max(1u, new_h));
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

    // 4. Initial idle state: empty input region so surface is 100% click-through
    window->set_input_region({});
    window->resize(420, 1);

    // 5. Main TxUI event and frame loop
    auto last_time = std::chrono::steady_clock::now();
    bool had_bubbles = false;

    while (!window->should_close() && g_running) {
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                window->close();
            } else {
                stack_widget->handle_event(event);
            }
        }

        // Always sync incoming notifications from queue
        stack_widget->sync_active_notifications();

        auto now = std::chrono::steady_clock::now();
        auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
        last_time = now;

        if (stack_widget->has_active_bubbles()) {
            had_bubbles = true;
            stack_widget->update(delta_ms);
            window->set_input_region(stack_widget->get_input_rects());
            window->request_repaint();
            window->present();
            window->wait_timeout(16);
        } else {
            if (had_bubbles) {
                // Transitioned from active bubbles to empty: clear input region and collapse window
                had_bubbles = false;
                window->set_input_region({});
                window->resize(420, 1);
                window->request_repaint();
                window->present();
            }
            window->wait_timeout(100);
        }
    }

    log::info("[Notifications] Daemon shut down cleanly.");
    return 0;
}
