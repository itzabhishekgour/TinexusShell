#include <txui/widgets/Label.hpp>
#include <txui/render/FontMetrics.hpp>

namespace txui {

void Label::set_text(std::string text) noexcept {
    if (m_text != text) {
        m_text = std::move(text);
        mark_needs_measure();
        mark_needs_paint();
    }
}

void Label::set_color(Color color) noexcept {
    if (m_color != color) {
        m_color = color;
        mark_needs_paint();
    }
}

void Label::set_font_size(double size) noexcept {
    if (m_font_size != size) {
        m_font_size = size;
        mark_needs_measure();
        mark_needs_paint();
    }
}

Size Label::measure_override(const Constraints& constraints) noexcept {
    const TextExtents extents = FontMetrics::measure(m_text, m_font_size);
    return constraints.constrain(Size(extents.width, extents.height));
}

void Label::paint_override(Painter& painter) const noexcept {
    // Render text centered vertically in the frame
    Point pos(frame().left(), frame().top() + (frame().height() - m_font_size) / 2.0);
    
    std::string text_to_draw = m_text;
    const TextExtents full_extents = FontMetrics::measure(m_text, m_font_size);
    
    if (full_extents.width > frame().width() && m_text.length() > 3) {
        size_t low = 0, high = m_text.length();
        size_t best = 0;
        while (low <= high) {
            size_t mid = low + (high - low) / 2;
            std::string test = m_text.substr(0, mid) + "...";
            const TextExtents test_extents = FontMetrics::measure(test, m_font_size);
            if (test_extents.width <= frame().width()) {
                best = mid;
                low = mid + 1;
            } else {
                if (mid == 0) break;
                high = mid - 1;
            }
        }
        if (best > 0) {
            text_to_draw = m_text.substr(0, best) + "...";
        } else {
            text_to_draw = "...";
        }
    }

    painter.draw_text(pos, text_to_draw, m_color, m_font_size);
}

} // namespace txui
