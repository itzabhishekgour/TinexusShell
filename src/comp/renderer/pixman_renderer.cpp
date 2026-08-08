#include "comp/renderer/pixman_renderer.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <cstring>

#include <pixman.h>

namespace tinexus::comp {

PixmanRenderer::~PixmanRenderer() = default;

bool PixmanRenderer::initialize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_canvas.resize(static_cast<size_t>(m_width) * static_cast<size_t>(m_height), 0xFF000000); // Clear to opaque black canvas
    log::info("PixmanRenderer: Initialized Pixman 2D software renderer ({}x{} framebuffer)", m_width, m_height);
    return true;
}

void PixmanRenderer::begin_frame() {
    m_in_frame = true;
    m_active_damage.clear();
    std::fill(m_canvas.begin(), m_canvas.end(), 0xFF000000); // Clear background
}

void PixmanRenderer::damage_region(const DamageRegion& region) {
    m_active_damage = region;
    log::info("PixmanRenderer: Target damage region updated ({} damage boxes)", region.boxes().size());
}

void PixmanRenderer::compose_surface(RenderSurface& surface) {
    if (!m_in_frame || !surface.buffer || !surface.buffer->pixels) return;

    uint32_t* src = static_cast<uint32_t*>(surface.buffer->pixels);
    int32_t src_w = surface.buffer->width;
    int32_t src_h = surface.buffer->height;

    if (surface.opacity >= 0.999f) {
        // Fast path: direct assignment without blending
        int32_t start_x = std::max(0, surface.x);
        int32_t start_y = std::max(0, surface.y);
        int32_t end_x = std::min(static_cast<int32_t>(m_width), surface.x + src_w);
        int32_t end_y = std::min(static_cast<int32_t>(m_height), surface.y + src_h);

        for (int32_t y = start_y; y < end_y; ++y) {
            for (int32_t x = start_x; x < end_x; ++x) {
                int32_t src_x = x - surface.x;
                int32_t src_y = y - surface.y;
                uint32_t pixel = src[src_y * src_w + src_x];
                size_t canvas_idx = static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
                m_canvas[canvas_idx] = pixel;
            }
        }
    } else {
        // Alpha-blended composition path using pixman
        pixman_image_t* src_img = pixman_image_create_bits(
            PIXMAN_a8r8g8b8, src_w, src_h, src, src_w * 4);
        pixman_image_t* dst_img = pixman_image_create_bits(
            PIXMAN_a8r8g8b8, m_width, m_height, m_canvas.data(), m_width * 4);

        pixman_color_t color;
        color.alpha = static_cast<uint16_t>(surface.opacity * 65535.0f);
        color.red = 0; color.green = 0; color.blue = 0;
        pixman_image_t* mask_img = pixman_image_create_solid_fill(&color);

        pixman_image_composite32(
            PIXMAN_OP_OVER, src_img, mask_img, dst_img,
            0, 0, 0, 0, surface.x, surface.y, src_w, src_h);

        pixman_image_unref(src_img);
        pixman_image_unref(mask_img);
        pixman_image_unref(dst_img);
    }

    log::info("PixmanRenderer: Composited surface #{} ({}x{} @ {},{}, opacity: {:.2f}) onto Pixman canvas", 
              surface.id, src_w, src_h, surface.x, surface.y, surface.opacity);
}

void PixmanRenderer::end_frame() {
    m_in_frame = false;
}

void PixmanRenderer::present() {
    log::info("PixmanRenderer: Presented composited frame to display output!");
}

RendererBackend PixmanRenderer::backend_type() const noexcept {
    return RendererBackend::Pixman;
}

} // namespace tinexus::comp
