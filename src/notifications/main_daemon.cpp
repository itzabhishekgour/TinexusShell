// ============================================================================
// main_daemon.cpp — tinexus-notifications daemon
// ============================================================================
// Headless D-Bus daemon implementing org.freedesktop.Notifications via sd-bus.
// Serves notification dispatching, DND policies, and notification history.
// UI rendering is handled via Wayland/CSD or shell (NotificationFlyout.qml).
// ============================================================================

#include "notifications/notification_server.hpp"
#include "notifications/notification_manager.hpp"
#include "notifications/dbus_server.hpp"
#include <common/logger.hpp>

#include <csignal>
#include <atomic>
#include <systemd/sd-bus.h>

using namespace tinexus;

static std::atomic<bool> g_running{true};

static void signal_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    log::set_component_name("notifications");
    log::info("[Notifications] Tinexus Notification Center Daemon starting (sd-bus org.freedesktop.Notifications)...");

    if (!notifications::DBusServer::instance().start()) {
        log::error("[Notifications] Failed to start DBus Server!");
        return 1;
    }

    sd_bus* bus = notifications::DBusServer::instance().bus();
    if (!bus) {
        log::error("[Notifications] No valid sd-bus handle!");
        return 1;
    }

    log::info("[Notifications] Entering sd-bus event loop...");

    while (g_running) {
        int r = sd_bus_process(bus, nullptr);
        if (r < 0) {
            log::error("[Notifications] Error processing bus: {}", strerror(-r));
            break;
        }
        if (r > 0) {
            // More requests pending, loop immediately
            continue;
        }

        r = sd_bus_wait(bus, static_cast<uint64_t>(-1));
        if (r < 0 && -r != EINTR) {
            log::error("[Notifications] Error waiting on bus: {}", strerror(-r));
            break;
        }
    }

    notifications::DBusServer::instance().stop();
    log::info("[Notifications] Daemon shut down cleanly.");
    return 0;
}
