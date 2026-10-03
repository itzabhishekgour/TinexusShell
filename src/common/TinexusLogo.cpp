#include "common/TinexusLogo.hpp"
#include "common/logger.hpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "common/stb_image.h"
#pragma GCC diagnostic pop

#include "common/tinexus_logo_data.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <mutex>

namespace tinexus::logo {

namespace {

LogoBuffer decode_raw_rgba_to_argb32(const uint8_t* raw, int w, int h) {
    if (!raw || w <= 0 || h <= 0) return {};

    LogoBuffer buf;
    buf.width = static_cast<uint32_t>(w);
    buf.height = static_cast<uint32_t>(h);
    buf.pixels = std::make_shared<std::vector<uint32_t>>(static_cast<size_t>(w) * static_cast<size_t>(h));

    // Convert RGBA to premultiplied ARGB32
    for (size_t i = 0; i < buf.pixels->size(); ++i) {
        uint32_t a = raw[i * 4 + 3];
        uint32_t r = (static_cast<uint32_t>(raw[i * 4 + 0]) * a) / 255U;
        uint32_t g = (static_cast<uint32_t>(raw[i * 4 + 1]) * a) / 255U;
        uint32_t b = (static_cast<uint32_t>(raw[i * 4 + 2]) * a) / 255U;
        (*buf.pixels)[i] = (a << 24) |
                           (r << 16) |
                           (g << 8)  |
                            b;
    }
    return buf;
}

LogoBuffer load_from_paths_or_memory(const std::vector<std::string>& filepaths,
                                     const unsigned char* mem_data,
                                     unsigned int mem_len) {
    int w = 0, h = 0, comp = 0;
    uint8_t* raw = nullptr;

    for (const auto& path : filepaths) {
        std::ifstream f(path, std::ios::binary);
        if (f.good()) {
            f.close();
            raw = stbi_load(path.c_str(), &w, &h, &comp, 4);
            if (raw) {
                log::info("[Logo] Loaded official Tinexus logo from {}", path);
                break;
            }
        }
    }

    if (!raw && mem_data && mem_len > 0) {
        raw = stbi_load_from_memory(mem_data, static_cast<int>(mem_len), &w, &h, &comp, 4);
        if (raw) {
            log::info("[Logo] Loaded official Tinexus logo from embedded binary asset ({}x{})", w, h);
        }
    }

    if (!raw) {
        log::error("[Logo] Failed to load official Tinexus logo from disk or memory!");
        return {};
    }

    LogoBuffer result = decode_raw_rgba_to_argb32(raw, w, h);
    stbi_image_free(raw);
    return result;
}

} // namespace

LogoBuffer get_logo(uint32_t target_size) {
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lock(s_mutex);

    static LogoBuffer s_logo_32;
    static LogoBuffer s_logo_large;

    if (target_size <= 32) {
        if (!s_logo_32.is_valid()) {
            std::vector<std::string> paths = {
                "/usr/share/icons/hicolor/32x32/apps/tinexus-logo.png",
                "assets/logo/tinexus-logo-32.png",
                "../assets/logo/tinexus-logo-32.png",
                "../../assets/logo/tinexus-logo-32.png"
            };
            s_logo_32 = load_from_paths_or_memory(paths,
                                                  logo_data::assets_logo_tinexus_logo_32_png,
                                                  logo_data::assets_logo_tinexus_logo_32_png_len);
        }
        return s_logo_32;
    } else {
        if (!s_logo_large.is_valid()) {
            std::vector<std::string> paths = {
                "/usr/share/tinexus/tinexus-logo.png",
                "/usr/share/pixmaps/tinexus.png",
                "/usr/share/icons/hicolor/256x256/apps/tinexus-logo.png",
                "/usr/share/icons/hicolor/128x128/apps/tinexus-logo.png",
                "assets/logo/tinexus-logo-256.png",
                "assets/logo/tinexus-logo.png",
                "../assets/logo/tinexus-logo-256.png",
                "../assets/logo/tinexus-logo.png",
                "../../assets/logo/tinexus-logo-256.png",
                "../../assets/logo/tinexus-logo.png",
                "/tinexus-logo.png"
            };
            s_logo_large = load_from_paths_or_memory(paths,
                                                     logo_data::assets_logo_tinexus_logo_128_png,
                                                     logo_data::assets_logo_tinexus_logo_128_png_len);
        }
        return s_logo_large;
    }
}

} // namespace tinexus::logo
