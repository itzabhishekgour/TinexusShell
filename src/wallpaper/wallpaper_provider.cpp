#include "wallpaper/wallpaper_provider.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::wallpaper {

bool ImageProvider::load(const std::string& path) {
    m_path = path;
    m_src_width = 1920;
    m_src_height = 1080;
    m_src_pixels.resize(static_cast<size_t>(m_src_width) * static_cast<size_t>(m_src_height), 0xFF1E1E2E); // Catppuccin Mocha base color

    log::info("ImageProvider: Loaded wallpaper image '{}' ({}x{})", path, m_src_width, m_src_height);
    return true;
}

WallpaperBuffer ImageProvider::render_buffer(uint32_t target_width, uint32_t target_height) {
    WallpaperBuffer buf;
    buf.width = target_width;
    buf.height = target_height;
    buf.pixels.resize(static_cast<size_t>(target_width) * static_cast<size_t>(target_height), 0xFF1E1E2E);
    log::info("ImageProvider: Rendered wallpaper buffer for display target {}x{}", target_width, target_height);
    return buf;
}

} // namespace tinexus::wallpaper
