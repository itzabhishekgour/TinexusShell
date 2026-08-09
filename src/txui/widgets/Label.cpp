#include <txui/widgets/Label.hpp>

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
    // Basic approximation since we don't have a FreeType TextMeasurer exposed directly to Widgets yet.
    // Assuming roughly 0.6 width-to-height ratio for proportional fonts.
    double width = m_text.length() * (m_font_size * 0.6);
    double height = m_font_size * 1.2; // Line height
    return constraints.constrain(Size(width, height));
}

void Label::paint_override(Painter& painter) const noexcept {
    // Render text centered vertically in the frame
    Point pos(frame().left(), frame().top() + (frame().height() - m_font_size) / 2.0);
    painter.draw_text(pos, m_text, m_color, m_font_size);
}

} // namespace txui
