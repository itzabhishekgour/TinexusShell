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
    if (!std::filesystem::exists(config_path, ec)) {
        log::info("ConfigStore: Config file not found. Generating defaults...");
        m_settings = generate_defaults();
        save_settings_atomic(config_path);
        return true;
    }

    // In a full TOML parser, version checks and migrations run here
    m_settings = generate_defaults();
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

    out << "version = " << m_settings.version << "\n\n"
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
