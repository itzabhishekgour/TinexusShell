#include "settings/settings_daemon.hpp"
#include "settings/schema_validator.hpp"
#include "common/logger.hpp"

namespace tinexus::settings {

SettingsDaemon& SettingsDaemon::instance() noexcept {
    static SettingsDaemon s_instance;
    return s_instance;
}

bool SettingsDaemon::initialize(const std::filesystem::path& config_path) {
    log::info("SettingsDaemon: Initializing settings daemon with config file '{}'...", config_path.string());
    return ConfigStore::instance().load_settings(config_path);
}

void SettingsDaemon::broadcast_settings_changed(const std::string& category) {
    log::info("SettingsDaemon: Broadcasting SETTINGS_CHANGED IPC notification for category '{}'...", category);
}

bool SettingsDaemon::update_theme(const std::string& new_theme) {
    if (!SchemaValidator::validate_theme(new_theme)) {
        log::error("SettingsDaemon: Invalid theme '{}' rejected by SchemaValidator", new_theme);
        return false;
    }

    auto settings = ConfigStore::instance().get_settings();
    settings.theme = new_theme;
    ConfigStore::instance().update_settings(settings);

    const char* home = std::getenv("HOME");
    std::filesystem::path config_path = home ? std::filesystem::path(home) / ".config/tinexus/settings.toml"
                                             : std::filesystem::path("/home/user/.config/tinexus/settings.toml");

    if (ConfigStore::instance().save_settings_atomic(config_path)) {
        broadcast_settings_changed("appearance");
        return true;
    }
    return false;
}

bool SettingsDaemon::update_scale(float new_scale) {
    if (!SchemaValidator::validate_scale(new_scale)) {
        log::error("SettingsDaemon: Invalid scale '{}' rejected by SchemaValidator", new_scale);
        return false;
    }

    auto settings = ConfigStore::instance().get_settings();
    settings.display_scale = new_scale;
    ConfigStore::instance().update_settings(settings);

    const char* home = std::getenv("HOME");
    std::filesystem::path config_path = home ? std::filesystem::path(home) / ".config/tinexus/settings.toml"
                                             : std::filesystem::path("/home/user/.config/tinexus/settings.toml");

    if (ConfigStore::instance().save_settings_atomic(config_path)) {
        broadcast_settings_changed("display");
        return true;
    }
    return false;
}

} // namespace tinexus::settings
