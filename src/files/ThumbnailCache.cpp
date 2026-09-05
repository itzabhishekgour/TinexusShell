#include "files/ThumbnailCache.hpp"
#include "common/logger.hpp"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO_NOT_REALLY
#include "files/stb_image.h"
#include <algorithm>
#include <cmath>

namespace tinexus::files {

ThumbnailCache& ThumbnailCache::instance() {
    static ThumbnailCache s_instance;
    return s_instance;
}

void ThumbnailCache::clear() {
    m_cache.clear();
}

std::shared_ptr<ImageThumbnail> ThumbnailCache::get_thumbnail(const std::filesystem::path& path, uint32_t target_max_dim) {
    if (target_max_dim == 0) return nullptr;

    std::string cache_key = path.string() + "@" + std::to_string(target_max_dim);
    auto it = m_cache.find(cache_key);
    if (it != m_cache.end()) {
        return it->second;
    }

    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) {
        return nullptr;
    }

    std::string ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".bmp" && ext != ".tga" && ext != ".webp") {
        return nullptr;
    }

    int src_w = 0, src_h = 0, channels = 0;
    unsigned char* data = stbi_load(path.string().c_str(), &src_w, &src_h, &channels, 4);
    if (!data || src_w <= 0 || src_h <= 0) {
        if (data) stbi_image_free(data);
        return nullptr;
    }

    // Calculate aspect ratio preserving destination dimensions
    uint32_t thumb_w = target_max_dim;
    uint32_t thumb_h = target_max_dim;

    if (src_w >= src_h) {
        thumb_w = target_max_dim;
        thumb_h = std::max(1u, static_cast<uint32_t>(src_h * (static_cast<double>(target_max_dim) / static_cast<double>(src_w))));
    } else {
        thumb_h = target_max_dim;
        thumb_w = std::max(1u, static_cast<uint32_t>(src_w * (static_cast<double>(target_max_dim) / static_cast<double>(src_h))));
    }

    auto pixels = std::make_shared<std::vector<uint32_t>>(static_cast<size_t>(thumb_w) * static_cast<size_t>(thumb_h));

    const double scale_x = static_cast<double>(src_w) / static_cast<double>(thumb_w);
    const double scale_y = static_cast<double>(src_h) / static_cast<double>(thumb_h);

    for (uint32_t dy = 0; dy < thumb_h; ++dy) {
        int sy = std::clamp(static_cast<int>(dy * scale_y), 0, src_w > 0 ? src_h - 1 : 0);
        for (uint32_t dx = 0; dx < thumb_w; ++dx) {
            int sx = std::clamp(static_cast<int>(dx * scale_x), 0, src_w > 0 ? src_w - 1 : 0);
            const unsigned char* p = data + ((sy * src_w + sx) * 4);
            uint8_t r = p[0];
            uint8_t g = p[1];
            uint8_t b = p[2];
            uint8_t a = p[3];

            // Premultiply alpha
            uint32_t pr = (static_cast<uint32_t>(r) * a) / 255u;
            uint32_t pg = (static_cast<uint32_t>(g) * a) / 255u;
            uint32_t pb = (static_cast<uint32_t>(b) * a) / 255u;

            (*pixels)[dy * thumb_w + dx] = (static_cast<uint32_t>(a) << 24) |
                                            (pr << 16) |
                                            (pg << 8)  |
                                            pb;
        }
    }

    stbi_image_free(data);

    auto result = std::make_shared<ImageThumbnail>();
    result->pixels = pixels;
    result->width = thumb_w;
    result->height = thumb_h;

    // Limit cache size to 128 thumbnails
    if (m_cache.size() >= 128) {
        m_cache.erase(m_cache.begin());
    }
    m_cache[cache_key] = result;

    return result;
}

} // namespace tinexus::files
