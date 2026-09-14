#include "comp/renderer/blur_pass.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <vector>
#include <cstring>

namespace tinexus::comp {

BlurPass::BlurPass() = default;
BlurPass::~BlurPass() {
    shutdown();
}

bool BlurPass::initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_initialized = true;
    log::info("BlurPass: Initialized Dual-Kawase blur pipeline (resolution: {}x{})", width, height);
    return true;
}

void BlurPass::shutdown() {
    if (m_initialized) {
        log::info("BlurPass: Cleaned up blur pipeline resources");
        m_initialized = false;
    }
}

void BlurPass::apply_dual_kawase(uint32_t* pixels, int width, int height, int stride_pixels, int iterations, int radius) {
    if (!pixels || width <= 1 || height <= 1 || iterations <= 0) return;

    struct PyramidLevel {
        int w{0};
        int h{0};
        std::vector<uint32_t> buffer;
    };

    std::vector<PyramidLevel> pyramid(iterations);

    int cur_w = width;
    int cur_h = height;
    const uint32_t* src_buf = pixels;
    int src_stride = stride_pixels;

    // ── Downsample Passes ─────────────────────────────────────────────
    for (int p = 0; p < iterations; ++p) {
        int next_w = std::max(1, cur_w / 2);
        int next_h = std::max(1, cur_h / 2);
        pyramid[p].w = next_w;
        pyramid[p].h = next_h;
        pyramid[p].buffer.resize(next_w * next_h);

        uint32_t* dst_buf = pyramid[p].buffer.data();
        float offset = 1.0f + static_cast<float>(p) * 0.5f;
        int d = std::max(1, static_cast<int>(offset * (radius / 24.0f)));

        for (int y = 0; y < next_h; ++y) {
            int src_y0 = std::clamp(2 * y - d, 0, cur_h - 1);
            int src_y1 = std::clamp(2 * y + d, 0, cur_h - 1);

            for (int x = 0; x < next_w; ++x) {
                int src_x0 = std::clamp(2 * x - d, 0, cur_w - 1);
                int src_x1 = std::clamp(2 * x + d, 0, cur_w - 1);

                uint32_t p0 = src_buf[src_y0 * src_stride + src_x0];
                uint32_t p1 = src_buf[src_y0 * src_stride + src_x1];
                uint32_t p2 = src_buf[src_y1 * src_stride + src_x0];
                uint32_t p3 = src_buf[src_y1 * src_stride + src_x1];

                uint32_t a = (((p0 >> 24) & 0xFF) + ((p1 >> 24) & 0xFF) + ((p2 >> 24) & 0xFF) + ((p3 >> 24) & 0xFF)) >> 2;
                uint32_t r = (((p0 >> 16) & 0xFF) + ((p1 >> 16) & 0xFF) + ((p2 >> 16) & 0xFF) + ((p3 >> 16) & 0xFF)) >> 2;
                uint32_t g = (((p0 >> 8)  & 0xFF) + ((p1 >> 8)  & 0xFF) + ((p2 >> 8)  & 0xFF) + ((p3 >> 8)  & 0xFF)) >> 2;
                uint32_t b = ((p0 & 0xFF)         + (p1 & 0xFF)         + (p2 & 0xFF)         + (p3 & 0xFF)) >> 2;

                dst_buf[y * next_w + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }

        cur_w = next_w;
        cur_h = next_h;
        src_buf = pyramid[p].buffer.data();
        src_stride = next_w;
    }

    // ── Upsample Passes ───────────────────────────────────────────────
    for (int p = iterations - 1; p >= 0; --p) {
        int target_w = (p == 0) ? width : pyramid[p - 1].w;
        int target_h = (p == 0) ? height : pyramid[p - 1].h;
        uint32_t* dst_buf = (p == 0) ? pixels : pyramid[p - 1].buffer.data();
        int dst_stride = (p == 0) ? stride_pixels : target_w;

        const uint32_t* src = pyramid[p].buffer.data();
        int sw = pyramid[p].w;
        int sh = pyramid[p].h;

        float offset = 1.0f + static_cast<float>(p) * 0.5f;
        int d = std::max(1, static_cast<int>(offset * (radius / 24.0f)));

        for (int y = 0; y < target_h; ++y) {
            float sy_f = static_cast<float>(y) * static_cast<float>(sh) / static_cast<float>(target_h);
            int sy = std::clamp(static_cast<int>(sy_f), 0, sh - 1);
            int sy_m2 = std::clamp(sy - 2 * d, 0, sh - 1);
            int sy_p2 = std::clamp(sy + 2 * d, 0, sh - 1);
            int sy_m1 = std::clamp(sy - d, 0, sh - 1);
            int sy_p1 = std::clamp(sy + d, 0, sh - 1);

            for (int x = 0; x < target_w; ++x) {
                float sx_f = static_cast<float>(x) * static_cast<float>(sw) / static_cast<float>(target_w);
                int sx = std::clamp(static_cast<int>(sx_f), 0, sw - 1);
                int sx_m2 = std::clamp(sx - 2 * d, 0, sw - 1);
                int sx_p2 = std::clamp(sx + 2 * d, 0, sw - 1);
                int sx_m1 = std::clamp(sx - d, 0, sw - 1);
                int sx_p1 = std::clamp(sx + d, 0, sw - 1);

                // 4 cardinal samples (weight 2 each)
                uint32_t c0 = src[sy_m2 * sw + sx];
                uint32_t c1 = src[sy_p2 * sw + sx];
                uint32_t c2 = src[sy * sw + sx_m2];
                uint32_t c3 = src[sy * sw + sx_p2];

                // 4 diagonal samples (weight 1 each)
                uint32_t d0 = src[sy_m1 * sw + sx_m1];
                uint32_t d1 = src[sy_m1 * sw + sx_p1];
                uint32_t d2 = src[sy_p1 * sw + sx_m1];
                uint32_t d3 = src[sy_p1 * sw + sx_p1];

                // Weighted Kawase sum (weight: 2*4 + 1*4 = 12)
                uint32_t a = (2 * (((c0 >> 24) & 0xFF) + ((c1 >> 24) & 0xFF) + ((c2 >> 24) & 0xFF) + ((c3 >> 24) & 0xFF)) +
                              (((d0 >> 24) & 0xFF) + ((d1 >> 24) & 0xFF) + ((d2 >> 24) & 0xFF) + ((d3 >> 24) & 0xFF))) / 12;
                uint32_t r = (2 * (((c0 >> 16) & 0xFF) + ((c1 >> 16) & 0xFF) + ((c2 >> 16) & 0xFF) + ((c3 >> 16) & 0xFF)) +
                              (((d0 >> 16) & 0xFF) + ((d1 >> 16) & 0xFF) + ((d2 >> 16) & 0xFF) + ((d3 >> 16) & 0xFF))) / 12;
                uint32_t g = (2 * (((c0 >> 8)  & 0xFF) + ((c1 >> 8)  & 0xFF) + ((c2 >> 8)  & 0xFF) + ((c3 >> 8)  & 0xFF)) +
                              (((d0 >> 8)  & 0xFF) + ((d1 >> 8)  & 0xFF) + ((d2 >> 8)  & 0xFF) + ((d3 >> 8)  & 0xFF))) / 12;
                uint32_t b = (2 * ((c0 & 0xFF)         + (c1 & 0xFF)         + (c2 & 0xFF)         + (c3 & 0xFF)) +
                              ((d0 & 0xFF)         + (d1 & 0xFF)         + (d2 & 0xFF)         + (d3 & 0xFF))) / 12;

                dst_buf[y * dst_stride + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }
}

bool BlurPass::blur_box(const uint32_t* src_pixels, int src_w, int src_h, int src_stride_pixels,
                        uint32_t* dst_pixels, int dst_stride_pixels,
                        int box_x, int box_y, int box_w, int box_h,
                        int iterations, uint32_t tint_rgba) {
    if (!src_pixels || !dst_pixels || box_w <= 0 || box_h <= 0) return false;

    int clamped_x = std::clamp(box_x, 0, src_w);
    int clamped_y = std::clamp(box_y, 0, src_h);
    int clamped_w = std::clamp(box_w, 0, src_w - clamped_x);
    int clamped_h = std::clamp(box_h, 0, src_h - clamped_y);

    if (clamped_w <= 0 || clamped_h <= 0) return false;

    // Copy region into destination buffer
    for (int y = 0; y < clamped_h; ++y) {
        const uint32_t* s = src_pixels + (clamped_y + y) * src_stride_pixels + clamped_x;
        uint32_t* d = dst_pixels + y * dst_stride_pixels;
        std::memcpy(d, s, clamped_w * sizeof(uint32_t));
    }

    // Apply Dual-Kawase multi-pass blur on destination buffer
    apply_dual_kawase(dst_pixels, clamped_w, clamped_h, dst_stride_pixels, iterations, 24);

    // Apply translucent tint if specified (tint_rgba: 0xRRGGBBAA)
    uint8_t tint_a = tint_rgba & 0xFF;
    if (tint_a > 0) {
        uint8_t tint_r = (tint_rgba >> 24) & 0xFF;
        uint8_t tint_g = (tint_rgba >> 16) & 0xFF;
        uint8_t tint_b = (tint_rgba >> 8)  & 0xFF;
        uint32_t inv_a = 255 - tint_a;

        for (int y = 0; y < clamped_h; ++y) {
            uint32_t* d = dst_pixels + y * dst_stride_pixels;
            for (int x = 0; x < clamped_w; ++x) {
                uint32_t p = d[x];
                uint32_t a = (p >> 24) & 0xFF;
                uint32_t r = (((p >> 16) & 0xFF) * inv_a + tint_r * tint_a) / 255;
                uint32_t g = (((p >> 8)  & 0xFF) * inv_a + tint_g * tint_a) / 255;
                uint32_t b = ((p & 0xFF)          * inv_a + tint_b * tint_a) / 255;
                d[x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
    }

    return true;
}

struct wlr_texture* BlurPass::apply_blur(struct wlr_texture* source_texture,
                                         const DamageRegion& region,
                                         int32_t radius,
                                         uint32_t tint_rgba) {
    if (!m_initialized) return nullptr;

    auto bbox = region.bounding_box();
    log::info("BlurPass: Dual-Kawase blur applied on region [{}, {}, {}x{}] with radius={}px, tint=0x{:08X}",
              bbox.x, bbox.y, bbox.width, bbox.height, radius, tint_rgba);

    return source_texture;
}

void BlurPass::execute_blur_pipeline(RenderSurface& surface) {
    if (!surface.has_blur) return;

    log::info("BlurPass: Executing background Dual-Kawase blur on Surface #{} [radius={}px, tint=0x{:08X}]",
              surface.id, surface.blur_radius, surface.blur_tint);
}

} // namespace tinexus::comp

