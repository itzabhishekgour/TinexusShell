#ifndef TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP
#define TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP

#include "settings/config_store.hpp"
#include <cstdint>
#include <string>

struct sd_bus;
struct sd_bus_slot;

namespace tinexus::settings {

class SettingsDaemon {
public:
    static SettingsDaemon& instance() noexcept;

    SettingsDaemon() = default;
    ~SettingsDaemon() = default;

    bool initialize(const std::filesystem::path& config_path);
    bool start_dbus_service();
    void stop_dbus_service();
    void run();
    void stop();

    bool update_theme(const std::string& new_theme);
    bool update_scale(float new_scale);
    void broadcast_settings_changed(const std::string& category);

    // Live wallpaper change — persists to TOML and notifies all daemons via D-Bus.
    bool update_wallpaper(const std::string& new_path,
                          uint8_t  mode       = 0,
                          uint16_t fade_ms    = 500,
                          bool     is_dynamic = false);

private:
    sd_bus*      m_bus{nullptr};
    sd_bus_slot* m_slot_io{nullptr};
    sd_bus_slot* m_slot_root{nullptr};
    bool         m_running{false};
};

} // namespace tinexus::settings

#endif // TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP
