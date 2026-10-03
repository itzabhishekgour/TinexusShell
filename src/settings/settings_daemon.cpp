// ============================================================================
// settings_daemon.cpp — tinexus-settings
// ============================================================================
// Manages live platform configuration and broadcasts changes over D-Bus
// (io.tinexus.shell.Settings / io.tinexus.shell.Wallpaper) so all subscribed daemons
// (wallpaper, lock, shell) react instantly.
// ============================================================================
#include "settings/settings_daemon.hpp"
#include "settings/schema_validator.hpp"
#include "common/logger.hpp"
#include "common/DBusNames.hpp"

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
// Forward declarations of D-Bus methods
// ─────────────────────────────────────────────────────────────────────────────
static int method_get_value(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
static int method_set_value(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
static int method_get_all_settings(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
static int method_reload_config(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);
static int method_set_wallpaper(sd_bus_message* m, void* userdata, sd_bus_error* ret_error);

static const sd_bus_vtable settings_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetValue", "s", "v", method_get_value, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetValue", "sv", "b", method_set_value, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("GetAllSettings", "", "a{sv}", method_get_all_settings, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("ReloadConfig", "", "", method_reload_config, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetWallpaper", "sybq", "b", method_set_wallpaper, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("ConfigChanged", "sv", 0),
    SD_BUS_SIGNAL("ThemeChanged", "s", 0),
    SD_BUS_VTABLE_END
};

static int method_get_value(sd_bus_message* m, void*, sd_bus_error*) {
    const char* key = nullptr;
    int r = sd_bus_message_read(m, "s", &key);
    if (r < 0) return r;

    auto settings = ConfigStore::instance().get_settings();
    std::string k = key ? key : "";
    std::transform(k.begin(), k.end(), k.begin(), ::tolower);

    sd_bus_message* reply = nullptr;
    r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    if (k == "theme" || k == "currenttheme") {
        r = sd_bus_message_append(reply, "v", "s", settings.theme.c_str());
    } else if (k == "wallpaper" || k == "wallpaper_path" || k == "wallpaperpath") {
        r = sd_bus_message_append(reply, "v", "s", settings.wallpaper_path.c_str());
    } else if (k == "accent" || k == "accent_color" || k == "accentcolor") {
        r = sd_bus_message_append(reply, "v", "s", settings.accent_color.c_str());
    } else if (k == "scale" || k == "display_scale" || k == "displayscale") {
        r = sd_bus_message_append(reply, "v", "d", static_cast<double>(settings.display_scale));
    } else {
        r = sd_bus_message_append(reply, "v", "s", "");
    }

    if (r < 0) {
        sd_bus_message_unref(reply);
        return r;
    }
    return sd_bus_send(nullptr, reply, nullptr);
}

static int method_set_value(sd_bus_message* m, void*, sd_bus_error*) {
    const char* key = nullptr;
    int r = sd_bus_message_read(m, "s", &key);
    if (r < 0) return r;

    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_VARIANT, nullptr);
    if (r < 0) return r;

    char type = 0;
    const char* contents = nullptr;
    r = sd_bus_message_peek_type(m, &type, &contents);
    if (r < 0) return r;

    auto settings = ConfigStore::instance().get_settings();
    std::string k = key ? key : "";
    std::transform(k.begin(), k.end(), k.begin(), ::tolower);

    if (type == SD_BUS_TYPE_STRING) {
        const char* val = nullptr;
        sd_bus_message_read(m, "s", &val);
        if (val) {
            if (k == "theme" || k == "currenttheme") {
                settings.theme = val;
            } else if (k == "wallpaper" || k == "wallpaper_path" || k == "wallpaperpath") {
                settings.wallpaper_path = val;
                SettingsDaemon::instance().update_wallpaper(val);
            } else if (k == "accent" || k == "accent_color" || k == "accentcolor") {
                settings.accent_color = val;
            }
        }
    } else if (type == SD_BUS_TYPE_DOUBLE) {
        double val = 1.0;
        sd_bus_message_read(m, "d", &val);
        if (k == "scale" || k == "display_scale") {
            settings.display_scale = static_cast<float>(val);
        }
    }
    sd_bus_message_exit_container(m);

    ConfigStore::instance().update_settings(settings);
    ConfigStore::instance().save_settings_atomic(resolve_config_path());
    SettingsDaemon::instance().broadcast_settings_changed(k);

    return sd_bus_reply_method_return(m, "b", 1);
}

static int method_set_wallpaper(sd_bus_message* m, void*, sd_bus_error*) {
    const char* path = nullptr;
    uint8_t mode = 0;
    int dynamic = 0;
    uint16_t fade_ms = 500;

    int r = sd_bus_message_read(m, "sybq", &path, &mode, &dynamic, &fade_ms);
    if (r < 0) return r;

    bool ok = SettingsDaemon::instance().update_wallpaper(path ? path : "", mode, fade_ms, dynamic != 0);
    return sd_bus_reply_method_return(m, "b", ok ? 1 : 0);
}

static int method_get_all_settings(sd_bus_message* m, void*, sd_bus_error*) {
    sd_bus_message* reply = nullptr;
    int r = sd_bus_message_new_method_return(m, &reply);
    if (r < 0) return r;

    r = sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "{sv}");
    if (r < 0) {
        sd_bus_message_unref(reply);
        return r;
    }

    auto s = ConfigStore::instance().get_settings();

    auto append_entry_s = [&](const char* k, const std::string& val) {
        sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv");
        sd_bus_message_append(reply, "s", k);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_VARIANT, "s");
        sd_bus_message_append(reply, "s", val.c_str());
        sd_bus_message_close_container(reply);
        sd_bus_message_close_container(reply);
    };

    auto append_entry_d = [&](const char* k, double val) {
        sd_bus_message_open_container(reply, SD_BUS_TYPE_DICT_ENTRY, "sv");
        sd_bus_message_append(reply, "s", k);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_VARIANT, "d");
        sd_bus_message_append(reply, "d", val);
        sd_bus_message_close_container(reply);
        sd_bus_message_close_container(reply);
    };

    append_entry_s("theme", s.theme);
    append_entry_s("accent_color", s.accent_color);
    append_entry_s("wallpaper_path", s.wallpaper_path);
    append_entry_s("wallpaper_mode", s.wallpaper_mode);
    append_entry_d("display_scale", static_cast<double>(s.display_scale));

    sd_bus_message_close_container(reply);
    return sd_bus_send(nullptr, reply, nullptr);
}

static int method_reload_config(sd_bus_message* m, void*, sd_bus_error*) {
    ConfigStore::instance().load_settings(resolve_config_path());
    SettingsDaemon::instance().broadcast_settings_changed("all");
    return sd_bus_reply_method_return(m, "");
}

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

bool SettingsDaemon::start_dbus_service() {
    int r = sd_bus_open_user(&m_bus);
    if (r < 0 || !m_bus) {
        log::error("SettingsDaemon: Failed to connect to session bus: {}", std::strerror(-r));
        return false;
    }

    r = sd_bus_add_object_vtable(m_bus, &m_slot_io,
                                 tinexus::common::dbus::path::Settings,
                                 tinexus::common::dbus::interface::Settings,
                                 settings_vtable, this);
    if (r < 0) {
        log::error("SettingsDaemon: Failed to add {} vtable: {}",
                   tinexus::common::dbus::path::Settings, std::strerror(-r));
        stop_dbus_service();
        return false;
    }

    // Legacy paths and interfaces for backward compatibility
    sd_bus_add_object_vtable(m_bus, &m_slot_root,
                             tinexus::common::dbus::legacy::SettingsPath,
                             tinexus::common::dbus::legacy::Settings,
                             settings_vtable, this);
    sd_bus_add_object_vtable(m_bus, nullptr,
                             "/Settings",
                             tinexus::common::dbus::legacy::Settings,
                             settings_vtable, this);

    r = sd_bus_request_name(m_bus, tinexus::common::dbus::service::Settings, 0);
    if (r < 0) {
        log::warn("SettingsDaemon: Failed to acquire '{}': {}",
                  tinexus::common::dbus::service::Settings, std::strerror(-r));
    } else {
        log::info("SettingsDaemon: Acquired D-Bus service '{}'",
                  tinexus::common::dbus::service::Settings);
    }

    // Also acquire legacy service name
    sd_bus_request_name(m_bus, tinexus::common::dbus::legacy::Settings, 0);
    return true;
}

void SettingsDaemon::stop_dbus_service() {
    if (m_slot_root) { sd_bus_slot_unref(m_slot_root); m_slot_root = nullptr; }
    if (m_slot_io)   { sd_bus_slot_unref(m_slot_io);   m_slot_io = nullptr; }
    if (m_bus)       { sd_bus_flush_close_unref(m_bus); m_bus = nullptr; }
}

void SettingsDaemon::run() {
    m_running = true;
    log::info("[settings] Entering sd-bus event loop...");
    while (m_running && m_bus) {
        int r = sd_bus_process(m_bus, nullptr);
        if (r < 0) {
            log::error("[settings] Error processing bus: {}", std::strerror(-r));
            break;
        }
        if (r > 0) continue;

        r = sd_bus_wait(m_bus, static_cast<uint64_t>(-1));
        if (r < 0 && -r != EINTR) {
            log::error("[settings] Error waiting on bus: {}", std::strerror(-r));
            break;
        }
    }
    stop_dbus_service();
    log::info("[settings] SettingsDaemon event loop exited.");
}

void SettingsDaemon::stop() {
    m_running = false;
}

void SettingsDaemon::broadcast_settings_changed(const std::string& category) {
    log::info("[settings] Broadcasting settings change for category '{}' via D-Bus", category);

    sd_bus* bus = m_bus;
    bool opened_temp = false;
    if (!bus) {
        int r = sd_bus_open_user(&bus);
        if (r < 0 || !bus) {
            log::warn("[settings] Failed to connect to user session bus: {} (settings change not broadcast)",
                      std::strerror(-r));
            return;
        }
        opened_temp = true;
    }

    // 1. Emit ConfigChanged signal on canonical path and legacy paths
    sd_bus_emit_signal(bus,
                       tinexus::common::dbus::path::Settings,
                       tinexus::common::dbus::interface::Settings,
                       "ConfigChanged",
                       "sv",
                       category.c_str(),
                       "s", category.c_str());

    sd_bus_emit_signal(bus,
                       tinexus::common::dbus::legacy::SettingsPath,
                       tinexus::common::dbus::legacy::Settings,
                       "ConfigChanged",
                       "sv",
                       category.c_str(),
                       "s", category.c_str());

    sd_bus_emit_signal(bus,
                       "/Settings",
                       tinexus::common::dbus::legacy::Settings,
                       "ConfigChanged",
                       "sv",
                       category.c_str(),
                       "s", category.c_str());

    // 2. If theme changed, emit ThemeChanged signal
    if (category == "appearance" || category == "theme") {
        auto settings = ConfigStore::instance().get_settings();
        sd_bus_emit_signal(bus,
                           tinexus::common::dbus::path::Settings,
                           tinexus::common::dbus::interface::Settings,
                           "ThemeChanged",
                           "s",
                           settings.theme.c_str());

        sd_bus_emit_signal(bus,
                           tinexus::common::dbus::legacy::SettingsPath,
                           tinexus::common::dbus::legacy::Settings,
                           "ThemeChanged",
                           "s",
                           settings.theme.c_str());

        sd_bus_emit_signal(bus,
                           "/Settings",
                           tinexus::common::dbus::legacy::Settings,
                           "ThemeChanged",
                           "s",
                           settings.theme.c_str());

        log::info("[settings] ThemeChanged signal broadcast sent (theme='{}')", settings.theme);
    }

    if (opened_temp) {
        sd_bus_flush_close_unref(bus);
    }
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

    // 3. Single authoritative trigger: Invoke Wallpaper.SetWallpaper
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
                           tinexus::common::dbus::service::Wallpaper,
                           tinexus::common::dbus::path::Wallpaper,
                           tinexus::common::dbus::interface::Wallpaper,
                           "SetWallpaper",
                           &error,
                           &reply,
                           "sybq",
                           new_path.c_str(),
                           mode,
                           is_dynamic ? 1 : 0,
                           fade_ms);
    if (r < 0) {
        // Fallback to legacy service if needed
        sd_bus_error_free(&error);
        if (reply) { sd_bus_message_unref(reply); reply = nullptr; }
        r = sd_bus_call_method(bus,
                               tinexus::common::dbus::legacy::Wallpaper,
                               tinexus::common::dbus::legacy::WallpaperPath,
                               tinexus::common::dbus::legacy::Wallpaper,
                               "SetWallpaper",
                               &error,
                               &reply,
                               "sybq",
                               new_path.c_str(),
                               mode,
                               is_dynamic ? 1 : 0,
                               fade_ms);
    }

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
                       tinexus::common::dbus::path::Settings,
                       tinexus::common::dbus::interface::Settings,
                       "ConfigChanged",
                       "sv",
                       "wallpaper",
                       "s", new_path.c_str());

    sd_bus_emit_signal(bus,
                       tinexus::common::dbus::legacy::SettingsPath,
                       tinexus::common::dbus::legacy::Settings,
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
