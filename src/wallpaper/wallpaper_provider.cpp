#include "wallpaper/wallpaper_provider.hpp"
#include "common/logger.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "wallpaper/stb_image.h"
#include <algorithm>
#include <cmath>

namespace tinexus::wallpaper {

bool ImageProvider::load(const std::string& path) {
    m_path = path;
    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4); // Force 4 RGBA channels
    if (!data) {
        log::warn("ImageProvider: Could not load image '{}' ({}) - fallback to gradient", path, stbi_failure_reason());
        return false;
    }

    m_src_width = static_cast<uint32_t>(w);
    m_src_height = static_cast<uint32_t>(h);
    m_src_pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h));

    // Convert RGBA to Wayland ARGB32
    for (size_t i = 0; i < m_src_pixels.size(); ++i) {
        uint8_t r = data[i * 4 + 0];
        uint8_t g = data[i * 4 + 1];
        uint8_t b = data[i * 4 + 2];
        uint8_t a = data[i * 4 + 3];

        m_src_pixels[i] = (static_cast<uint32_t>(a) << 24) |
                          (static_cast<uint32_t>(r) << 16) |
                          (static_cast<uint32_t>(g) << 8)  |
                          static_cast<uint32_t>(b);
    }

    stbi_image_free(data);
    log::info("ImageProvider: Loaded custom wallpaper image '{}' ({}x{})", path, m_src_width, m_src_height);
    return true;
}

WallpaperBuffer ImageProvider::render_buffer(uint32_t target_width, uint32_t target_height) {
    WallpaperBuffer buf;
    buf.width = target_width;
    buf.height = target_height;
    buf.pixels.resize(static_cast<size_t>(target_width) * static_cast<size_t>(target_height));

    if (!m_src_pixels.empty() && m_src_width > 0 && m_src_height > 0) {
        // High quality nearest-neighbor scaling from m_src_pixels to target buffer
        const double scale_x = static_cast<double>(m_src_width) / static_cast<double>(target_width);
        const double scale_y = static_cast<double>(m_src_height) / static_cast<double>(target_height);

        for (uint32_t y = 0; y < target_height; ++y) {
            uint32_t src_y = std::min(static_cast<uint32_t>(y * scale_y), m_src_height - 1);
            uint32_t* dest_row = buf.pixels.data() + (y * target_width);
            const uint32_t* src_row = m_src_pixels.data() + (src_y * m_src_width);

            for (uint32_t x = 0; x < target_width; ++x) {
                uint32_t src_x = std::min(static_cast<uint32_t>(x * scale_x), m_src_width - 1);
                dest_row[x] = src_row[src_x];
            }
        }
        log::info("ImageProvider: Rendered custom image wallpaper buffer {}x{}", target_width, target_height);
        return buf;
    }

    // Fallback: Deep Teal Mountain Gradient
    const double cx = target_width * 0.5;
    const double cy = target_height * 0.5;
    const double max_r = std::hypot(cx, cy);

    for (uint32_t y = 0; y < target_height; ++y) {
        uint32_t* row = buf.pixels.data() + (y * target_width);
        double dy = y - cy;
        for (uint32_t x = 0; x < target_width; ++x) {
            double dx = x - cx;
            double r = std::hypot(dx, dy) / max_r;
            r = std::clamp(r, 0.0, 1.0);

            uint8_t red   = static_cast<uint8_t>(20.0 * (1.0 - r) + 10.0 * r);
            uint8_t green = static_cast<uint8_t>(58.0 * (1.0 - r) + 26.0 * r);
            uint8_t blue  = static_cast<uint8_t>(68.0 * (1.0 - r) + 32.0 * r);

            row[x] = (0xFFU << 24) | (red << 16) | (green << 8) | blue;
        }
    }

    log::info("ImageProvider: Rendered fallback gradient wallpaper {}x{}", target_width, target_height);
    return buf;
}

} // namespace tinexus::wallpaper
