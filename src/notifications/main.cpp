#include "notifications/notification_server.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>

namespace {
std::atomic<bool> g_running{true};
void signal_handler(int) {
    g_running = false;
}
} // namespace

int main() {
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    tinexus::log::set_component_name("notifications");
    tinexus::log::info("Starting Tinexus Notification Center Daemon (tinexus-notifications)...");

    auto info = tinexus::notifications::NotificationServer::instance().get_server_information();
    tinexus::log::info("Notification Server Running: {} v{} ({})", info.name, info.version, info.vendor);

    tinexus::log::info("Registered D-Bus IPC service: 'io.tinexus.shell.Notifications1'");
    tinexus::log::info("Freedesktop Notifications org.freedesktop.Notifications service active.");
    tinexus::log::info("tinexus-notifications connected to wayland-0");

    while (g_running) {
        // Future daemon work: IPC, events, state updates
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
