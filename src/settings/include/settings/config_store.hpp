#ifndef TINEXUS_SETTINGS_CONFIG_STORE_HPP
#define TINEXUS_SETTINGS_CONFIG_STORE_HPP

#include <string>
#include <filesystem>

namespace tinexus::settings {

struct PlatformSettings {
    uint32_t version{1};

    // Appearance
    std::string theme{"dark"};
    std::string accent_color{"#3B82F6"};
    float blur_opacity{0.85f};

    // Wallpaper
    std::string wallpaper_path{"/usr/share/backgrounds/tinexus.png"};
    std::string wallpaper_mode{"fill"};

    // Display
    float display_scale{1.0f};
    std::string resolution{"auto"};

    // Input
    std::string keymap{"us"};
    uint32_t key_repeat_delay{250};
    uint32_t key_repeat_rate{30};

    // Power
    uint32_t idle_sleep_timeout_mins{15};
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
