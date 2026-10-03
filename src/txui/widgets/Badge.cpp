#include <txui/widgets/Badge.hpp>
#include <algorithm>

namespace txui {

Badge::Badge(std::string text, Color bg, Color fg, double font_size) noexcept
    : m_text(std::move(text)),
      m_background_color(bg),
      m_text_color(fg),
      m_font_size(font_size) {}

void Badge::set_text(std::string text) noexcept {
    if (m_text != text) {
        m_text = std::move(text);
        mark_needs_measure();
        mark_needs_paint();
    }
}

void Badge::set_background_color(Color color) noexcept {
    if (m_background_color != color) {
        m_background_color = color;
        mark_needs_paint();
    }
}

void Badge::set_text_color(Color color) noexcept {
    if (m_text_color != color) {
        m_text_color = color;
        mark_needs_paint();
    }
}

void Badge::set_font_size(double size) noexcept {
    if (m_font_size != size) {
        m_font_size = size;
        mark_needs_measure();
        mark_needs_paint();
    }
}

void Badge::set_corner_radius(double radius) noexcept {
    if (m_corner_radius != radius) {
        m_corner_radius = radius;
        mark_needs_paint();
    }
}

void Badge::set_padding(double px, double py) noexcept {
    if (m_padding_x != px || m_padding_y != py) {
        m_padding_x = px;
        m_padding_y = py;
        mark_needs_measure();
        mark_needs_paint();
    }
}

Size Badge::measure_override(const Constraints& constraints) noexcept {
    const double text_w = FontMetrics::measure(m_text, m_font_size).width;
    const double w = text_w + m_padding_x * 2.0;
    const double h = m_font_size + m_padding_y * 2.0 + 4.0;
    return constraints.constrain(Size(w, h));
}

void Badge::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();
    if (f.width() <= 0.0 || f.height() <= 0.0 || m_text.empty()) return;

    painter.fill_rounded_rect(f, m_corner_radius, m_background_color);

    const double text_w = FontMetrics::measure(m_text, m_font_size).width;
    const double tx = f.x() + (f.width() - text_w) * 0.5;
    const double ty = f.y() + (f.height() - m_font_size) * 0.5;

    painter.draw_text(Point(tx, ty), m_text, m_text_color, m_font_size);
}

Rect Badge::render(Painter& painter, const Point& origin, const std::string& text,
                   const Color& bg, const Color& fg,
                   double font_size, double radius,
                   double pad_x, double pad_y) noexcept {
    if (text.empty()) {
        return Rect(origin.x, origin.y, 0.0, 0.0);
    }
    const double text_w = FontMetrics::measure(text, font_size).width;
    const double w = text_w + pad_x * 2.0;
    const double h = font_size + pad_y * 2.0 + 4.0;
    const Rect bounds(origin.x, origin.y, w, h);

    painter.fill_rounded_rect(bounds, radius, bg);

    const double tx = origin.x + pad_x;
    const double ty = origin.y + (h - font_size) * 0.5;
    painter.draw_text(Point(tx, ty), text, fg, font_size);

    return bounds;
}

} // namespace txui
