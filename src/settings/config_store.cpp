#include "settings/config_store.hpp"
#include "settings/schema_validator.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <unistd.h>
#include <fcntl.h>

namespace tinexus::settings {

ConfigStore& ConfigStore::instance() noexcept {
    static ConfigStore s_instance;
    return s_instance;
}

PlatformSettings ConfigStore::generate_defaults() const {
    return PlatformSettings{};
}

bool ConfigStore::load_settings(const std::filesystem::path& config_path) {
    log::info("ConfigStore: Loading configuration from '{}'", config_path.string());
    std::error_code ec;
    m_settings = generate_defaults();

    if (!std::filesystem::exists(config_path, ec)) {
        log::info("ConfigStore: Config file not found. Generating defaults...");
        save_settings_atomic(config_path);
        return true;
    }

    std::ifstream in(config_path);
    if (!in.is_open()) {
        return false;
    }

    auto trim = [](std::string s) {
        auto b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return std::string{};
        auto e = s.find_last_not_of(" \t\r\n");
        s = s.substr(b, e - b + 1);
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            s = s.substr(1, s.size() - 2);
        }
        return s;
    };

    std::string line;
    while (std::getline(in, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line = trim(line);
        if (line.empty() || (line.front() == '[' && line.back() == ']')) continue;

        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        try {
            if (key == "version") m_settings.version = static_cast<uint32_t>(std::stoul(val));
            else if (key == "theme" || key == "theme_mode") m_settings.theme = val;
            else if (key == "accent_color") m_settings.accent_color = val;
            else if (key == "accent_index") m_settings.accent_index = std::stoi(val);
            else if (key == "blur_opacity") m_settings.blur_opacity = std::stof(val);
            else if (key == "wallpaper_path" || key == "path") m_settings.wallpaper_path = val;
            else if (key == "mode" || key == "wallpaper_mode") m_settings.wallpaper_mode = val;
            else if (key == "selected_wallpaper_idx") m_settings.selected_wallpaper_idx = std::stoi(val);
            else if (key == "scale" || key == "display_scale") m_settings.display_scale = std::stof(val);
            else if (key == "display_scale_idx") m_settings.display_scale_idx = std::stoi(val);
            else if (key == "resolution") m_settings.resolution = val;
            else if (key == "night_light") m_settings.night_light = (val == "true" || val == "1");
            else if (key == "vrr_enabled") m_settings.vrr_enabled = (val == "true" || val == "1");
            else if (key == "screen_timeout_min") m_settings.screen_timeout_min = static_cast<uint32_t>(std::stoul(val));
            else if (key == "sleep_after_min" || key == "idle_sleep_timeout_mins") m_settings.idle_sleep_timeout_mins = static_cast<uint32_t>(std::stoul(val));
            else if (key == "power_profile_idx") m_settings.power_profile_idx = std::stoi(val);
            else if (key == "lock_on_sleep") m_settings.lock_on_sleep = (val == "true" || val == "1");
            else if (key == "pam_auth") m_settings.pam_auth = (val == "true" || val == "1");
            else if (key == "clipboard_history_size") m_settings.clipboard_history_size = std::stoi(val);
            else if (key == "guard_enabled") m_settings.guard_enabled = (val == "true" || val == "1");
            else if (key == "sandbox_enabled") m_settings.sandbox_enabled = (val == "true" || val == "1");
        } catch (...) {}
    }

    return true;
}

bool ConfigStore::save_settings_atomic(const std::filesystem::path& config_path) {
    log::info("ConfigStore: Executing atomic TOML write to '{}'...", config_path.string());

    std::filesystem::create_directories(config_path.parent_path());
    auto tmp_path = config_path.string() + ".tmp";

    std::ofstream out(tmp_path, std::ios::trunc);
    if (!out.is_open()) {
        log::error("ConfigStore: Failed to open temp config file '{}'", tmp_path);
        return false;
    }

    out << "version = " << m_settings.version << "\n"
        << "accent_index = " << m_settings.accent_index << "\n"
        << "selected_wallpaper_idx = " << m_settings.selected_wallpaper_idx << "\n"
        << "wallpaper_path = \"" << m_settings.wallpaper_path << "\"\n"
        << "theme_mode = \"" << m_settings.theme << "\"\n"
        << "display_scale_idx = " << m_settings.display_scale_idx << "\n"
        << "night_light = " << (m_settings.night_light ? "true" : "false") << "\n"
        << "vrr_enabled = " << (m_settings.vrr_enabled ? "true" : "false") << "\n"
        << "screen_timeout_min = " << m_settings.screen_timeout_min << "\n"
        << "sleep_after_min = " << m_settings.idle_sleep_timeout_mins << "\n"
        << "power_profile_idx = " << m_settings.power_profile_idx << "\n"
        << "lock_on_sleep = " << (m_settings.lock_on_sleep ? "true" : "false") << "\n"
        << "pam_auth = " << (m_settings.pam_auth ? "true" : "false") << "\n"
        << "clipboard_history_size = " << m_settings.clipboard_history_size << "\n"
        << "guard_enabled = " << (m_settings.guard_enabled ? "true" : "false") << "\n"
        << "sandbox_enabled = " << (m_settings.sandbox_enabled ? "true" : "false") << "\n\n"
        << "[appearance]\n"
        << "theme = \"" << m_settings.theme << "\"\n"
        << "accent_color = \"" << m_settings.accent_color << "\"\n"
        << "blur_opacity = " << m_settings.blur_opacity << "\n\n"
        << "[wallpaper]\n"
        << "path = \"" << m_settings.wallpaper_path << "\"\n"
        << "mode = \"" << m_settings.wallpaper_mode << "\"\n\n"
        << "[display]\n"
        << "scale = " << m_settings.display_scale << "\n"
        << "resolution = \"" << m_settings.resolution << "\"\n";
    out.flush();

    // Flush OS file buffers via fsync
    int fd = open(tmp_path.c_str(), O_WRONLY);
    if (fd != -1) {
        fsync(fd);
        close(fd);
    }
    out.close();

    // Atomic rename
    std::error_code ec;
    std::filesystem::rename(tmp_path, config_path, ec);
    if (ec) {
        log::error("ConfigStore: Atomic rename failed: {}", ec.message());
        return false;
    }

    log::info("ConfigStore: Atomic config update committed successfully.");
    return true;
}

} // namespace tinexus::settings
