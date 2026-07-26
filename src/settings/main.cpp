#include "settings/settings_daemon.hpp"
#include "common/logger.hpp"
#include <iostream>

int main() {
    tinexus::log::set_component_name("settings");
    tinexus::log::info("Starting Tinexus Settings Daemon (tinexus-settings)...");

    const char* home = std::getenv("HOME");
    std::filesystem::path config_path = home ? std::filesystem::path(home) / ".config/tinexus/settings.toml"
                                             : std::filesystem::path("/home/user/.config/tinexus/settings.toml");

    if (!tinexus::settings::SettingsDaemon::instance().initialize(config_path)) {
        tinexus::log::error("Failed to initialize Settings Daemon!");
        return 1;
    }

    tinexus::log::info("Tinexus Settings Daemon listening for live configuration changes.");
    return 0;
}
