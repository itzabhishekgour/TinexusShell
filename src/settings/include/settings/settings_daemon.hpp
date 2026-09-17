#ifndef TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP
#define TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP

#include "settings/config_store.hpp"
#include <cstdint>
#include <string>

namespace tinexus::settings {

class SettingsDaemon {
public:
    static SettingsDaemon& instance() noexcept;

    SettingsDaemon() = default;
    ~SettingsDaemon() = default;

    bool initialize(const std::filesystem::path& config_path);
    bool update_theme(const std::string& new_theme);
    bool update_scale(float new_scale);
    void broadcast_settings_changed(const std::string& category);

    // Live wallpaper change — persists to TOML and notifies all daemons via D-Bus.
    // new_path : absolute path to image or .twallpaper dir (empty = dynamic schedule)
    // mode     : 0=fill, 1=fit, 2=center, 3=tile, 4=stretch
    // fade_ms  : cross-fade duration in ms (0 = instant)
    // is_dynamic: true if new_path is a .twallpaper bundle
    bool update_wallpaper(const std::string& new_path,
                          uint8_t  mode       = 0,
                          uint16_t fade_ms    = 500,
                          bool     is_dynamic = false);
};

} // namespace tinexus::settings

#endif // TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP
