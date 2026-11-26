#ifndef TINEXUS_SETTINGS_CONFIG_STORE_HPP
#define TINEXUS_SETTINGS_CONFIG_STORE_HPP

#include <string>
#include <filesystem>

namespace tinexus::settings {

struct PlatformSettings {
    uint32_t version{1};

    // Appearance
    std::string theme{"dark"};
    std::string accent_color{"#00c3ff"};
    int accent_index{0};
    float blur_opacity{0.85f};

    // Wallpaper
    std::string wallpaper_path{"/usr/share/backgrounds/emerald-matrix.png"};
    std::string wallpaper_mode{"fill"};
    int selected_wallpaper_idx{0};

    // Display
    float display_scale{1.0f};
    int display_scale_idx{0};
    std::string resolution{"auto"};
    bool night_light{false};
    bool vrr_enabled{false};

    // Input
    std::string keymap{"us"};
    uint32_t key_repeat_delay{250};
    uint32_t key_repeat_rate{30};

    // Power & System
    uint32_t screen_timeout_min{15};
    uint32_t idle_sleep_timeout_mins{30};
    int power_profile_idx{1};
    bool lock_on_sleep{true};
    bool pam_auth{true};
    int clipboard_history_size{50};

    // Security & Privacy
    bool guard_enabled{true};
    bool sandbox_enabled{true};
};

class ConfigStore {
public:
    static ConfigStore& instance() noexcept;

    ConfigStore() = default;
    ~ConfigStore() = default;

    bool load_settings(const std::filesystem::path& config_path);
    bool save_settings_atomic(const std::filesystem::path& config_path);
    PlatformSettings generate_defaults() const;

    [[nodiscard]] const PlatformSettings& get_settings() const noexcept { return m_settings; }
    void update_settings(const PlatformSettings& new_settings) { m_settings = new_settings; }

private:
    PlatformSettings m_settings;
};

} // namespace tinexus::settings

#endif // TINEXUS_SETTINGS_CONFIG_STORE_HPP
