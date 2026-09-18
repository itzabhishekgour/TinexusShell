#include "settings/settings_daemon.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <csignal>

static void signal_handler(int) {
    tinexus::settings::SettingsDaemon::instance().stop();
}

int main() {
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    tinexus::log::set_component_name("settings");
    tinexus::log::info("Starting Tinexus Settings Daemon (tinexus-settings)...");

    const char* home = std::getenv("HOME");
    std::filesystem::path config_path = home ? std::filesystem::path(home) / ".config/tinexus/settings.toml"
                                             : std::filesystem::path("/home/user/.config/tinexus/settings.toml");

    if (!tinexus::settings::SettingsDaemon::instance().initialize(config_path)) {
        tinexus::log::error("Failed to initialize Settings Daemon!");
        return 1;
    }

    if (!tinexus::settings::SettingsDaemon::instance().start_dbus_service()) {
        tinexus::log::error("Failed to start Settings D-Bus service!");
        return 1;
    }

    tinexus::log::info("Tinexus Settings Daemon listening for live configuration changes.");
    tinexus::settings::SettingsDaemon::instance().run();

    tinexus::log::info("Tinexus Settings Daemon shut down cleanly.");
    return 0;
}
