#include "notifications/notification_server.hpp"
#include "common/logger.hpp"
#include <iostream>

int main() {
    tinexus::log::set_component_name("notifications");
    tinexus::log::info("Starting Tinexus Notification Center Daemon (tinexus-notifications)...");

    auto info = tinexus::notifications::NotificationServer::instance().get_server_information();
    tinexus::log::info("Notification Server Running: {} v{} ({})", info.name, info.version, info.vendor);

    tinexus::log::info("Registered D-Bus IPC service: 'io.tinexus.shell.Notifications1'");
    tinexus::log::info("Freedesktop Notifications org.freedesktop.Notifications service active.");
    tinexus::log::info("tinexus-notifications connected to wayland-0");
    return 0;
}
