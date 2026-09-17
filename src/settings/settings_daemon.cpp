// ============================================================================
// settings_daemon.cpp — tinexus-settings
// ============================================================================
// Manages live platform configuration and broadcasts changes over D-Bus
// (io.tinexus.Settings / io.tinexus.Wallpaper) so all subscribed daemons
// (wallpaper, lock, shell) react instantly.
// ============================================================================
#include "settings/settings_daemon.hpp"
#include "settings/schema_validator.hpp"
#include "common/logger.hpp"

#include <systemd/sd-bus.h>

#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <cerrno>

namespace tinexus::settings {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers — anonymous namespace, not part of public API
// ─────────────────────────────────────────────────────────────────────────────
namespace {

/// Resolve the settings.toml path cleanly, with no hardcoded usernames.
[[nodiscard]] std::filesystem::path resolve_config_path() {
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::filesystem::path(home) / ".config/tinexus/settings.toml";
    }
    // System-wide fallback — no username, no hardcoded UID
    return std::filesystem::path("/etc/tinexus/settings.toml");
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// SettingsDaemon — public interface
// ─────────────────────────────────────────────────────────────────────────────

SettingsDaemon& SettingsDaemon::instance() noexcept {
    static SettingsDaemon s_instance;
    return s_instance;
}

bool SettingsDaemon::initialize(const std::filesystem::path& config_path) {
    log::info("SettingsDaemon: Initializing with config '{}'...", config_path.string());
    return ConfigStore::instance().load_settings(config_path);
}

// ─────────────────────────────────────────────────────────────────────────────
// broadcast_settings_changed — D-Bus signal broadcast
//
// Emits ConfigChanged and ThemeChanged signals on io.tinexus.Settings.
// Subscribers (wallpaper, lock, dock, shell) receive these signals immediately.
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDaemon::broadcast_settings_changed(const std::string& category) {
    log::info("[settings] Broadcasting settings change for category '{}' via D-Bus", category);

    sd_bus* bus = nullptr;
    int r = sd_bus_open_user(&bus);
    if (r < 0 || !bus) {
        log::warn("[settings] Failed to connect to user session bus: {} (settings change not broadcast)",
                  std::strerror(-r));
        return;
    }

    // 1. Emit ConfigChanged signal on /io/tinexus/Settings and /Settings
    sd_bus_emit_signal(bus,
                       "/io/tinexus/Settings",
                       "io.tinexus.Settings",
                       "ConfigChanged",
                       "sv",
                       category.c_str(),
                       "s", category.c_str());

    sd_bus_emit_signal(bus,
                       "/Settings",
                       "io.tinexus.Settings",
                       "ConfigChanged",
                       "sv",
                       category.c_str(),
                       "s", category.c_str());

    // 2. If theme changed, emit ThemeChanged signal
    if (category == "appearance" || category == "theme") {
        auto settings = ConfigStore::instance().get_settings();
        sd_bus_emit_signal(bus,
                           "/io/tinexus/Settings",
                           "io.tinexus.Settings",
                           "ThemeChanged",
                           "s",
                           settings.theme.c_str());

        sd_bus_emit_signal(bus,
                           "/Settings",
                           "io.tinexus.Settings",
                           "ThemeChanged",
                           "s",
                           settings.theme.c_str());

        log::info("[settings] ThemeChanged signal broadcast sent (theme='{}')", settings.theme);
    }

    sd_bus_flush_close_unref(bus);
}

// ─────────────────────────────────────────────────────────────────────────────
// update_wallpaper — live wallpaper change with instant D-Bus propagation
//
// Steps:
//  1. Validate path exists on disk (empty path = use dynamic schedule).
//  2. Update ConfigStore + atomically write settings.toml (write→fsync→rename).
//  3. Single authoritative trigger: Invoke io.tinexus.Wallpaper.SetWallpaper
//     method over session bus to command the renderer directly.
// ─────────────────────────────────────────────────────────────────────────────
bool SettingsDaemon::update_wallpaper(const std::string& new_path,
                                      uint8_t  mode,
                                      uint16_t fade_ms,
                                      bool     is_dynamic) {
    // 1. Validate — non-empty paths must exist on disk right now
    if (!new_path.empty()) {
        std::error_code ec;
        if (!std::filesystem::exists(new_path, ec) || ec) {
            log::error("[settings] update_wallpaper: path not found: '{}'", new_path);
            return false;
        }
    }

    // 2. Persist to ConfigStore and atomically write settings.toml
    auto settings = ConfigStore::instance().get_settings();
    if (!new_path.empty()) {
        settings.wallpaper_path = new_path;
    }
    const char* mode_strings[] = {"fill", "fit", "center", "tile", "stretch"};
    settings.wallpaper_mode = (mode < 5) ? mode_strings[mode] : "fill";
    ConfigStore::instance().update_settings(settings);

    if (!ConfigStore::instance().save_settings_atomic(resolve_config_path())) {
        log::error("[settings] Atomic write of settings.toml failed after wallpaper change");
        return false;
    }

    // 3. Single authoritative trigger: Invoke io.tinexus.Wallpaper.SetWallpaper
    sd_bus* bus = nullptr;
    int r = sd_bus_open_user(&bus);
    if (r < 0 || !bus) {
        log::warn("[settings] D-Bus session bus unreachable: {} "
                  "(settings saved, wallpaper will apply on next restart)",
                  std::strerror(-r));
        return true;
    }

    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    r = sd_bus_call_method(bus,
                           "io.tinexus.Wallpaper",
                           "/io/tinexus/Wallpaper",
                           "io.tinexus.Wallpaper",
                           "SetWallpaper",
                           &error,
                           &reply,
                           "sybq",
                           new_path.c_str(),
                           mode,
                           is_dynamic ? 1 : 0,
                           fade_ms);
    if (r < 0) {
        log::warn("[settings] SetWallpaper D-Bus call failed: {} "
                  "(wallpaper daemon may not be running yet; settings saved)",
                  error.message ? error.message : std::strerror(-r));
    } else {
        log::info("[settings] SetWallpaper invoked via D-Bus (path='{}', mode={}, fade_ms={}, dynamic={})",
                  new_path, mode, fade_ms, is_dynamic ? 1 : 0);
    }

    // Also broadcast ConfigChanged for UI binding
    sd_bus_emit_signal(bus,
                       "/io/tinexus/Settings",
                       "io.tinexus.Settings",
                       "ConfigChanged",
                       "sv",
                       "wallpaper",
                       "s", new_path.c_str());

    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_flush_close_unref(bus);

    return true;
}

bool SettingsDaemon::update_theme(const std::string& new_theme) {
    if (!SchemaValidator::validate_theme(new_theme)) {
        log::error("SettingsDaemon: Invalid theme '{}' rejected by SchemaValidator", new_theme);
        return false;
    }

    auto settings = ConfigStore::instance().get_settings();
    settings.theme = new_theme;
    ConfigStore::instance().update_settings(settings);

    if (ConfigStore::instance().save_settings_atomic(resolve_config_path())) {
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

    if (ConfigStore::instance().save_settings_atomic(resolve_config_path())) {
        broadcast_settings_changed("display");
        return true;
    }
    return false;
}

} // namespace tinexus::settings
