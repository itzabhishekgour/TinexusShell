#include "painter.hpp"
#include <algorithm>

namespace ui {

void ShmPainter::begin(SurfaceBuffer* buffer) {
    m_buffer = buffer;
}

void ShmPainter::end() {
    m_buffer = nullptr;
}

void ShmPainter::fill_rect(const Rect& rect, const Color& color) {
    if (!m_buffer) return;

    int bw = m_buffer->width();
    int bh = m_buffer->height();

    int start_x = std::clamp(rect.x, 0, bw);
    int start_y = std::clamp(rect.y, 0, bh);
    int end_x = std::clamp(rect.x + rect.width, 0, bw);
    int end_y = std::clamp(rect.y + rect.height, 0, bh);

    uint32_t argb = color.to_argb8888();
    uint32_t* pixels = m_buffer->pixels();

    for (int y = start_y; y < end_y; ++y) {
        for (int x = start_x; x < end_x; ++x) {
            pixels[y * bw + x] = argb;
        }
    }
}

void ShmPainter::clear(const Color& color) {
    if (!m_buffer) return;
    
    Rect r{0, 0, m_buffer->width(), m_buffer->height()};
    fill_rect(r, color);
}

} // namespace ui
