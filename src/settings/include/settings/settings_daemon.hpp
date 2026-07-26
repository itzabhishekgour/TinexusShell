#ifndef TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP
#define TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP

#include "settings/config_store.hpp"
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
};

} // namespace tinexus::settings

#endif // TINEXUS_SETTINGS_SETTINGS_DAEMON_HPP
