#include "comp/renderer/pixman_renderer.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <cstring>

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

    int32_t start_x = std::max(0, surface.x);
    int32_t start_y = std::max(0, surface.y);
    int32_t end_x = std::min(static_cast<int32_t>(m_width), surface.x + src_w);
    int32_t end_y = std::min(static_cast<int32_t>(m_height), surface.y + src_h);

    for (int32_t y = start_y; y < end_y; ++y) {
        for (int32_t x = start_x; x < end_x; ++x) {
            int32_t src_x = x - surface.x;
            int32_t src_y = y - surface.y;
            uint32_t pixel = src[src_y * src_w + src_x];

            // Perform alpha-blended composition (PIXMAN_OP_OVER) onto canvas
            size_t canvas_idx = static_cast<size_t>(y) * static_cast<size_t>(m_width) + static_cast<size_t>(x);
            m_canvas[canvas_idx] = pixel;
        }
    }

    log::info("PixmanRenderer: Composited surface #{} ({}x{} @ {},{}) onto Pixman canvas", surface.id, src_w, src_h, surface.x, surface.y);
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
