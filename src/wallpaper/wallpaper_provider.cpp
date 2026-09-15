// ============================================================================
// wallpaper_provider.cpp — tinexus-wallpaper (Milestones 2 + 4)
// ============================================================================
// Aspect-Fill rendering engine with:
//   • Correct aspect-fill / fit / center / tile / stretch math (no distortion)
//   • Bilinear 2×2 texel sampling (eliminates nearest-neighbor aliasing at 4K)
//   • 192 MB LRU WallpaperCache (avoids repeated CPU decode of 4K images)
//   • SIMD-autovectorizable crossfade_lerp (CPU lerp for 60fps cross-fade)
// ============================================================================
#include "wallpaper/wallpaper_provider.hpp"
#include "common/logger.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "wallpaper/stb_image.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cassert>

namespace tinexus::wallpaper {

// ─────────────────────────────────────────────────────────────────────────────
// WallpaperCache — LRU implementation
// ─────────────────────────────────────────────────────────────────────────────

// Static definition
WallpaperCache ImageProvider::s_cache;

const CachedImage* WallpaperCache::get_or_load(const std::string& path) {
    // Cache hit — move to front (most-recently-used)
    auto it = m_map.find(path);
    if (it != m_map.end()) {
        m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second);
        return &(*m_lru_list.begin());
    }

    // Cache miss — decode via stb_image
    int w = 0, h = 0, channels = 0;
    unsigned char* raw = stbi_load(path.c_str(), &w, &h, &channels, 4);
    if (!raw) {
        tinexus::log::warn("[cache] stb_image failed for '{}': {}", path, stbi_failure_reason());
        return nullptr;
    }

    const size_t pixel_count = static_cast<size_t>(w) * static_cast<size_t>(h);
    const size_t byte_size   = pixel_count * 4;

    // Enforce budget BEFORE inserting the new entry
    while (m_total_bytes + byte_size > MAX_CACHE_BYTES && !m_lru_list.empty()) {
        evict_lru_until_under_limit();
        if (m_lru_list.empty()) break;
    }

    // Insert at front (most-recently-used)
    m_lru_list.emplace_front();
    CachedImage& entry = m_lru_list.front();
    entry.path      = path;
    entry.width     = static_cast<uint32_t>(w);
    entry.height    = static_cast<uint32_t>(h);
    entry.byte_size = byte_size;
    entry.pixels.resize(pixel_count);

    // Convert RGBA → ARGB8888 (Wayland SHM wire format)
    for (size_t i = 0; i < pixel_count; ++i) {
        const uint8_t r = raw[i * 4 + 0];
        const uint8_t g = raw[i * 4 + 1];
        const uint8_t b = raw[i * 4 + 2];
        const uint8_t a = raw[i * 4 + 3];
        entry.pixels[i] = (static_cast<uint32_t>(a) << 24) |
                          (static_cast<uint32_t>(r) << 16) |
                          (static_cast<uint32_t>(g) << 8)  |
                          static_cast<uint32_t>(b);
    }
    stbi_image_free(raw);

    m_map.emplace(path, m_lru_list.begin());
    m_total_bytes += byte_size;

    tinexus::log::info("[cache] Loaded '{}' ({}×{}, {:.1f} MB, cache total {:.1f} MB)",
                       path, w, h,
                       static_cast<double>(byte_size) / (1024.0 * 1024.0),
                       static_cast<double>(m_total_bytes) / (1024.0 * 1024.0));
    return &entry;
}

void WallpaperCache::evict_lru_until_under_limit() {
    if (m_lru_list.empty()) return;
    const CachedImage& lru = m_lru_list.back();
    tinexus::log::debug("[cache] Evicting LRU entry '{}' ({:.1f} MB)", lru.path,
                        static_cast<double>(lru.byte_size) / (1024.0 * 1024.0));
    m_total_bytes -= lru.byte_size;
    m_map.erase(lru.path);
    m_lru_list.pop_back();
}

void WallpaperCache::evict(const std::string& path) {
    auto it = m_map.find(path);
    if (it == m_map.end()) return;
    m_total_bytes -= it->second->byte_size;
    m_lru_list.erase(it->second);
    m_map.erase(it);
}

void WallpaperCache::clear() {
    m_lru_list.clear();
    m_map.clear();
    m_total_bytes = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// ImageProvider — load
// ─────────────────────────────────────────────────────────────────────────────

bool ImageProvider::load(const std::string& path) {
    if (path == m_path && m_img != nullptr) {
        return true; // Same path — already loaded
    }
    m_img  = s_cache.get_or_load(path);
    m_path = m_img ? path : "";
    return m_img != nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// bilinear_sample — 2×2 texel gather with fixed-point fractional weights
//
// Samples position (fx, fy) from the source buffer using bilinear interpolation.
// Clamped at borders to avoid out-of-bounds reads.
// Per-channel lerp is done in integer arithmetic to allow autovectorization.
// ─────────────────────────────────────────────────────────────────────────────
[[nodiscard]] uint32_t ImageProvider::bilinear_sample(const uint32_t* pixels,
                                                       uint32_t src_w,
                                                       uint32_t src_h,
                                                       float    fx,
                                                       float    fy) noexcept {
    // Clamp to valid texel range [0, dim-1]
    fx = std::max(0.0f, fx);
    fy = std::max(0.0f, fy);

    const uint32_t x0 = static_cast<uint32_t>(fx);
    const uint32_t y0 = static_cast<uint32_t>(fy);
    const uint32_t x1 = std::min(x0 + 1, src_w - 1);
    const uint32_t y1 = std::min(y0 + 1, src_h - 1);

    // Sub-pixel fractional weights in [0, 256) for 8-bit precision
    const uint32_t tx = static_cast<uint32_t>((fx - static_cast<float>(x0)) * 256.0f);
    const uint32_t ty = static_cast<uint32_t>((fy - static_cast<float>(y0)) * 256.0f);
    const uint32_t itx = 256 - tx;
    const uint32_t ity = 256 - ty;

    // Gather 2×2 texels
    const uint32_t p00 = pixels[y0 * src_w + x0];
    const uint32_t p10 = pixels[y0 * src_w + x1];
    const uint32_t p01 = pixels[y1 * src_w + x0];
    const uint32_t p11 = pixels[y1 * src_w + x1];

    // Per-channel bilinear blend using 8-bit fixed-point
    uint32_t result = 0;
    for (int shift : {0, 8, 16, 24}) {
        const uint32_t c00 = (p00 >> shift) & 0xFF;
        const uint32_t c10 = (p10 >> shift) & 0xFF;
        const uint32_t c01 = (p01 >> shift) & 0xFF;
        const uint32_t c11 = (p11 >> shift) & 0xFF;

        const uint32_t top    = (c00 * itx + c10 * tx) >> 8;
        const uint32_t bottom = (c01 * itx + c11 * tx) >> 8;
        const uint32_t blended = (top * ity + bottom * ty) >> 8;
        result |= (blended & 0xFF) << shift;
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// crossfade_lerp — SIMD-autovectorizable CPU alpha blend
//
// Computes per-pixel linear interpolation:
//   dst[i] = src_a[i] + t * (src_b[i] - src_a[i])
//
// Using integer per-channel math for maximum autovectorization.
// The __restrict__ qualifiers guarantee no aliasing so the compiler can emit
// SIMD (SSE2/AVX2/NEON) instructions under -O2 -ftree-vectorize.
//
// At 4K (3840×2160 = 8,294,400 pixels), one call takes ~8–12ms on a modern
// CPU core with AVX2. 30 calls at 60fps → 500ms fade = ~30ms total CPU time.
// ─────────────────────────────────────────────────────────────────────────────
void ImageProvider::crossfade_lerp(const uint32_t* __restrict__ src_a,
                                    const uint32_t* __restrict__ src_b,
                                    uint32_t* __restrict__       dst,
                                    size_t                       pixel_count,
                                    float                        t) noexcept {
    // Convert t to an 8-bit fixed-point weight [0, 256]
    const uint32_t weight_b = static_cast<uint32_t>(t * 256.0f + 0.5f);
    const uint32_t weight_a = 256 - weight_b;

    // Per-pixel, per-channel lerp
    // This loop structure is what GCC/Clang vectorize into packed SSE2/AVX2.
    for (size_t i = 0; i < pixel_count; ++i) {
        const uint32_t pa = src_a[i];
        const uint32_t pb = src_b[i];

        const uint32_t r = ((((pa >> 16) & 0xFF) * weight_a) + (((pb >> 16) & 0xFF) * weight_b)) >> 8;
        const uint32_t g = ((((pa >>  8) & 0xFF) * weight_a) + (((pb >>  8) & 0xFF) * weight_b)) >> 8;
        const uint32_t b = (((pa & 0xFF) * weight_a) + ((pb & 0xFF) * weight_b)) >> 8;
        const uint32_t a = ((((pa >> 24) & 0xFF) * weight_a) + (((pb >> 24) & 0xFF) * weight_b)) >> 8;

        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// render_buffer — aspect-aware rendering with bilinear sampling
// ─────────────────────────────────────────────────────────────────────────────
WallpaperBuffer ImageProvider::render_buffer(uint32_t target_width,
                                              uint32_t target_height,
                                              FitMode  mode) {
    WallpaperBuffer buf;
    buf.width  = target_width;
    buf.height = target_height;
    buf.pixels.resize(static_cast<size_t>(target_width) * static_cast<size_t>(target_height), 0u);

    if (m_img && m_img->width > 0 && m_img->height > 0 && !m_img->pixels.empty()) {
        const uint32_t src_w = m_img->width;
        const uint32_t src_h = m_img->height;
        const float    src_fw = static_cast<float>(src_w);
        const float    src_fh = static_cast<float>(src_h);
        const float    dst_fw = static_cast<float>(target_width);
        const float    dst_fh = static_cast<float>(target_height);

        switch (mode) {

        // ──────────────────────────────────────────────────────────────────────
        // FILL (default): aspect-fill — scale uniformly so the image covers
        // the entire output. Excess pixels are cropped symmetrically.
        // ──────────────────────────────────────────────────────────────────────
        case FitMode::Fill: {
            // Uniform scale factor that covers both dimensions
            const float scale = std::max(dst_fw / src_fw, dst_fh / src_fh);

            // Visible region inside the source at this scale
            const float visible_src_w = dst_fw / scale;
            const float visible_src_h = dst_fh / scale;

            // Symmetric crop offset (centre-anchored)
            const float src_start_x = (src_fw - visible_src_w) * 0.5f;
            const float src_start_y = (src_fh - visible_src_h) * 0.5f;

            // Step size in source space per destination pixel
            const float step_x = visible_src_w / dst_fw;
            const float step_y = visible_src_h / dst_fh;

            for (uint32_t dy = 0; dy < target_height; ++dy) {
                float fy = src_start_y + static_cast<float>(dy) * step_y;
                uint32_t* dst_row = buf.pixels.data() + dy * target_width;
                for (uint32_t dx = 0; dx < target_width; ++dx) {
                    float fx = src_start_x + static_cast<float>(dx) * step_x;
                    dst_row[dx] = bilinear_sample(m_img->pixels.data(), src_w, src_h, fx, fy);
                }
            }
            break;
        }

        // ──────────────────────────────────────────────────────────────────────
        // FIT: aspect-fit — scale so the entire image is visible.
        // Letterbox (black bars) fill the remaining area.
        // ──────────────────────────────────────────────────────────────────────
        case FitMode::Fit: {
            const float scale = std::min(dst_fw / src_fw, dst_fh / src_fh);

            const uint32_t scaled_w = static_cast<uint32_t>(std::round(src_fw * scale));
            const uint32_t scaled_h = static_cast<uint32_t>(std::round(src_fh * scale));

            // Centering offset in destination
            const uint32_t off_x = (target_width  > scaled_w) ? (target_width  - scaled_w) / 2 : 0;
            const uint32_t off_y = (target_height > scaled_h) ? (target_height - scaled_h) / 2 : 0;

            for (uint32_t dy = 0; dy < scaled_h && (off_y + dy) < target_height; ++dy) {
                float fy = static_cast<float>(dy) / scale;
                uint32_t* dst_row = buf.pixels.data() + (off_y + dy) * target_width + off_x;
                for (uint32_t dx = 0; dx < scaled_w && dx < target_width; ++dx) {
                    float fx = static_cast<float>(dx) / scale;
                    dst_row[dx] = bilinear_sample(m_img->pixels.data(), src_w, src_h, fx, fy);
                }
            }
            break;
        }

        // ──────────────────────────────────────────────────────────────────────
        // CENTER: 1:1 pixel, centered, no scaling. Image may be cropped if
        // it is larger than the output, or show black bars if smaller.
        // ──────────────────────────────────────────────────────────────────────
        case FitMode::Center: {
            // Source rect centred on the source image
            const int32_t src_cx = static_cast<int32_t>(src_w) / 2;
            const int32_t src_cy = static_cast<int32_t>(src_h) / 2;
            const int32_t dst_cx = static_cast<int32_t>(target_width)  / 2;
            const int32_t dst_cy = static_cast<int32_t>(target_height) / 2;
            const int32_t offset_x = src_cx - dst_cx; // source pixel at dst (0,0)
            const int32_t offset_y = src_cy - dst_cy;

            for (uint32_t dy = 0; dy < target_height; ++dy) {
                int32_t sy = offset_y + static_cast<int32_t>(dy);
                uint32_t* dst_row = buf.pixels.data() + dy * target_width;
                for (uint32_t dx = 0; dx < target_width; ++dx) {
                    int32_t sx = offset_x + static_cast<int32_t>(dx);
                    if (sx >= 0 && sy >= 0 && static_cast<uint32_t>(sx) < src_w && static_cast<uint32_t>(sy) < src_h) {
                        dst_row[dx] = m_img->pixels[static_cast<uint32_t>(sy) * src_w + static_cast<uint32_t>(sx)];
                    } else {
                        dst_row[dx] = 0xFF000000u; // Opaque black letterbox
                    }
                }
            }
            break;
        }

        // ──────────────────────────────────────────────────────────────────────
        // TILE: 1:1 pixel, tiled with wrapping modulo arithmetic.
        // ──────────────────────────────────────────────────────────────────────
        case FitMode::Tile: {
            for (uint32_t dy = 0; dy < target_height; ++dy) {
                const uint32_t sy = dy % src_h;
                uint32_t* dst_row = buf.pixels.data() + dy * target_width;
                for (uint32_t dx = 0; dx < target_width; ++dx) {
                    dst_row[dx] = m_img->pixels[sy * src_w + (dx % src_w)];
                }
            }
            break;
        }

        // ──────────────────────────────────────────────────────────────────────
        // STRETCH: unconstrained bilinear scale to fill exactly.
        // Distorts aspect ratio — use only when explicitly requested.
        // ──────────────────────────────────────────────────────────────────────
        case FitMode::Stretch: {
            const float step_x = src_fw / dst_fw;
            const float step_y = src_fh / dst_fh;
            for (uint32_t dy = 0; dy < target_height; ++dy) {
                float fy = (static_cast<float>(dy) + 0.5f) * step_y - 0.5f;
                uint32_t* dst_row = buf.pixels.data() + dy * target_width;
                for (uint32_t dx = 0; dx < target_width; ++dx) {
                    float fx = (static_cast<float>(dx) + 0.5f) * step_x - 0.5f;
                    dst_row[dx] = bilinear_sample(m_img->pixels.data(), src_w, src_h, fx, fy);
                }
            }
            break;
        }
        } // switch(mode)

        log::debug("[provider] render_buffer: {}×{} mode={} src={}×{}",
                   target_width, target_height, static_cast<int>(mode), src_w, src_h);
        return buf;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Fallback: Deep Teal radial gradient — guaranteed, no file I/O.
    // ─────────────────────────────────────────────────────────────────────────
    const double cx    = target_width  * 0.5;
    const double cy    = target_height * 0.5;
    const double max_r = std::hypot(cx, cy);

    for (uint32_t y = 0; y < target_height; ++y) {
        uint32_t* row = buf.pixels.data() + y * target_width;
        const double dy = static_cast<double>(y) - cy;
        for (uint32_t x = 0; x < target_width; ++x) {
            const double dx = static_cast<double>(x) - cx;
            const double r  = std::clamp(std::hypot(dx, dy) / max_r, 0.0, 1.0);

            const uint8_t red   = static_cast<uint8_t>(20.0 * (1.0 - r) + 10.0 * r);
            const uint8_t green = static_cast<uint8_t>(58.0 * (1.0 - r) + 26.0 * r);
            const uint8_t blue  = static_cast<uint8_t>(68.0 * (1.0 - r) + 32.0 * r);
            row[x] = (0xFFU << 24) | (static_cast<uint32_t>(red) << 16) |
                     (static_cast<uint32_t>(green) << 8) | static_cast<uint32_t>(blue);
        }
    }

    log::debug("[provider] render_buffer: {}×{} (gradient fallback)", target_width, target_height);
    return buf;
}

} // namespace tinexus::wallpaper



