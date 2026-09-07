#include <txui/widgets/Button.hpp>
#include <txui/input/Event.hpp>
#include <algorithm>

namespace txui {

Button::Button(std::string text, std::function<void()> on_click) noexcept
    : m_text(std::move(text)), m_on_click(std::move(on_click)) {}

Button::Button(std::string text, IconType icon, std::function<void()> on_click) noexcept
    : m_text(std::move(text)), m_icon(icon), m_on_click(std::move(on_click)) {}

void Button::set_text(std::string text) noexcept {
    if (m_text != text) {
        m_text = std::move(text);
        mark_needs_measure();
        mark_needs_paint();
    }
}

void Button::set_icon(std::optional<IconType> icon) noexcept {
    if (m_icon != icon) {
        m_icon = icon;
        mark_needs_measure();
        mark_needs_paint();
    }
}

void Button::set_style(Style style) noexcept {
    if (m_style != style) {
        m_style = style;
        mark_needs_paint();
    }
}

void Button::set_corner_radius(double radius) noexcept {
    if (m_corner_radius != radius) {
        m_corner_radius = radius;
        mark_needs_paint();
    }
}

void Button::set_font_size(double size) noexcept {
    if (m_font_size != size) {
        m_font_size = size;
        mark_needs_measure();
        mark_needs_paint();
    }
}

void Button::set_enabled(bool enabled) noexcept {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!m_enabled) {
            m_hovered = false;
            m_pressed = false;
        }
        mark_needs_paint();
    }
}

Size Button::measure_override(const Constraints& constraints) noexcept {
    double content_w = 0.0;
    if (!m_text.empty()) {
        const auto extents = FontMetrics::measure(m_text, m_font_size);
        content_w += extents.width;
    }
    if (m_icon.has_value()) {
        content_w += 18.0; // Icon width
        if (!m_text.empty()) {
            content_w += 8.0; // Spacing between icon and text
        }
    }

    const double width = content_w + 32.0; // 16px left + 16px right padding
    const double height = std::max(36.0, m_font_size + 18.0);
    return constraints.constrain(Size(width, height));
}

void Button::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();

    Color bg_color;
    Color fg_color(255, 255, 255, 255);
    Color border_color(255, 255, 255, 25);

    if (!m_enabled) {
        bg_color = Color(255, 255, 255, 8);
        fg_color = Color(255, 255, 255, 90);
        border_color = Color(255, 255, 255, 12);
    } else {
        switch (m_style) {
            case Style::Primary:
                if (m_pressed) {
                    bg_color = Color(0, 100, 215, 255);
                } else if (m_hovered) {
                    bg_color = Color(30, 144, 255, 255);
                } else {
                    bg_color = Color(0, 122, 255, 240);
                }
                border_color = Color(255, 255, 255, 40);
                break;
            case Style::Danger:
                if (m_pressed) {
                    bg_color = Color(220, 40, 30, 255);
                } else if (m_hovered) {
                    bg_color = Color(255, 80, 70, 255);
                } else {
                    bg_color = Color(255, 59, 48, 220);
                }
                border_color = Color(255, 255, 255, 35);
                break;
            case Style::Ghost:
                if (m_pressed) {
                    bg_color = Color(255, 255, 255, 30);
                } else if (m_hovered) {
                    bg_color = Color(255, 255, 255, 18);
                } else {
                    bg_color = Color(0, 0, 0, 0);
                }
                border_color = Color(0, 0, 0, 0);
                break;
            case Style::Standard:
            default:
                if (m_pressed) {
                    bg_color = Color(255, 255, 255, 45);
                } else if (m_hovered) {
                    bg_color = Color(255, 255, 255, 28);
                } else {
                    bg_color = Color(255, 255, 255, 18);
                }
                border_color = Color(255, 255, 255, 30);
                break;
        }
    }

    // Background pill/rect
    painter.fill_rounded_rect(f, m_corner_radius, bg_color);

    // Subtle 1px border stroke if non-ghost
    if (border_color.a() > 0) {
        // Outer line highlight
        painter.fill_rounded_rect(Rect(f.x(), f.y(), f.width(), 1.0), 1.0, border_color);
        painter.fill_rounded_rect(Rect(f.x(), f.y() + f.height() - 1.0, f.width(), 1.0), 1.0, Color(0, 0, 0, 50));
    }

    // Content centering
    double content_w = 0.0;
    TextExtents text_extents;
    if (!m_text.empty()) {
        text_extents = FontMetrics::measure(m_text, m_font_size);
        content_w += text_extents.width;
    }
    if (m_icon.has_value()) {
        content_w += 18.0;
        if (!m_text.empty()) {
            content_w += 8.0;
        }
    }

    double cur_x = f.x() + (f.width() - content_w) * 0.5;

    if (m_icon.has_value()) {
        const double icon_size = 18.0;
        const double icon_y = f.y() + (f.height() - icon_size) * 0.5;
        Icon icon_widget(*m_icon, icon_size);
        icon_widget.layout(Rect(cur_x, icon_y, icon_size, icon_size));
        icon_widget.paint(painter);
        cur_x += icon_size + 8.0;
    }

    if (!m_text.empty()) {
        const double text_y = f.y() + (f.height() - text_extents.height) * 0.5;
        painter.draw_text(Point(cur_x, text_y), m_text, fg_color, m_font_size, false, false, FontFamily::UI);
    }
}

bool Button::handle_event(const Event& event) noexcept {
    if (!m_enabled) return false;

    if (event.type == EventType::PointerMove) {
        Point p(event.pointer.x, event.pointer.y);
        bool inside = frame().contains(p);
        if (m_hovered != inside) {
            m_hovered = inside;
            mark_needs_paint();
        }
        return inside;
    } else if (event.type == EventType::PointerButtonPress) {
        Point p(event.pointer.x, event.pointer.y);
        if (frame().contains(p) && event.pointer.button == MouseButton::Left) {
            m_pressed = true;
            mark_needs_paint();
            return true;
        }
    } else if (event.type == EventType::PointerButtonRelease) {
        if (m_pressed && event.pointer.button == MouseButton::Left) {
            m_pressed = false;
            mark_needs_paint();
            Point p(event.pointer.x, event.pointer.y);
            if (frame().contains(p) && m_on_click) {
                m_on_click();
            }
            return true;
        }
    } else if (event.type == EventType::PointerLeave) {
        if (m_hovered || m_pressed) {
            m_hovered = false;
            m_pressed = false;
            mark_needs_paint();
        }
    }
    return false;
}

} // namespace txui
