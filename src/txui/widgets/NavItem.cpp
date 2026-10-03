#include <txui/widgets/NavItem.hpp>
#include <txui/input/Event.hpp>
#include <txui/render/FontMetrics.hpp>
#include <algorithm>

namespace txui {

NavItem::NavItem(std::string label, std::function<void()> on_click) noexcept
    : m_label(std::move(label)), m_on_click(std::move(on_click)) {}

NavItem::NavItem(std::string label, IconType icon, std::function<void()> on_click) noexcept
    : m_label(std::move(label)), m_icon_type(icon), m_on_click(std::move(on_click)) {}

NavItem::NavItem(std::string label, CustomIconRenderer custom_icon, std::function<void()> on_click) noexcept
    : m_label(std::move(label)), m_custom_icon_renderer(std::move(custom_icon)), m_on_click(std::move(on_click)) {}

void NavItem::set_label(std::string label) noexcept {
    if (m_label != label) {
        m_label = std::move(label);
        mark_needs_measure();
        mark_needs_paint();
    }
}

void NavItem::set_icon(std::optional<IconType> icon) noexcept {
    if (m_icon_type != icon) {
        m_icon_type = icon;
        mark_needs_paint();
    }
}

void NavItem::set_custom_icon_renderer(CustomIconRenderer renderer) noexcept {
    m_custom_icon_renderer = std::move(renderer);
    mark_needs_paint();
}

void NavItem::set_selected(bool selected) noexcept {
    if (m_selected != selected) {
        m_selected = selected;
        mark_needs_paint();
    }
}

void NavItem::set_accent_color(Color color) noexcept {
    if (m_accent_color != color) {
        m_accent_color = color;
        mark_needs_paint();
    }
}

void NavItem::set_font_size(double size) noexcept {
    if (m_font_size != size) {
        m_font_size = size;
        mark_needs_measure();
        mark_needs_paint();
    }
}

void NavItem::set_corner_radius(double radius) noexcept {
    if (m_corner_radius != radius) {
        m_corner_radius = radius;
        mark_needs_paint();
    }
}

void NavItem::set_enabled(bool enabled) noexcept {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!m_enabled) {
            m_hovered = false;
            m_pressed = false;
        }
        mark_needs_paint();
    }
}

Size NavItem::measure_override(const Constraints& constraints) noexcept {
    const double min_w = 120.0;
    const double h = std::max(42.0, m_font_size + 24.0);
    return constraints.constrain(Size(min_w, h));
}

void NavItem::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();
    if (f.width() <= 0.0 || f.height() <= 0.0) return;

    // Background pill
    if (m_selected) {
        const Color bg(m_accent_color.r(), m_accent_color.g(), m_accent_color.b(), 44);
        painter.fill_rounded_rect(f, m_corner_radius, bg);

        // Left accent indicator bar (3.5px wide with rounded cap)
        const Rect indicator(f.x() + 2.5, f.y() + 8.0, 3.5, f.height() - 16.0);
        painter.fill_rounded_rect(indicator, 1.75, m_accent_color);
    } else if (m_hovered && m_enabled) {
        const Color hover_bg(255, 255, 255, 18);
        painter.fill_rounded_rect(f, m_corner_radius, hover_bg);
    }

    // Icon
    const double icon_sz = 20.0;
    const double icon_x = f.x() + 14.0;
    const double icon_y = f.y() + (f.height() - icon_sz) * 0.5;
    const Rect icon_rect(icon_x, icon_y, icon_sz, icon_sz);

    if (m_custom_icon_renderer) {
        m_custom_icon_renderer(painter, icon_rect);
    } else if (m_icon_type) {
        Icon::render(painter, *m_icon_type, icon_rect);
    }

    // Text label
    Color text_col;
    if (!m_enabled) {
        text_col = Color(120, 125, 140, 150);
    } else if (m_selected) {
        text_col = Color(255, 255, 255, 255);
    } else if (m_hovered) {
        text_col = Color(245, 248, 255, 245);
    } else {
        text_col = Color(185, 190, 205, 220);
    }

    const double text_x = (m_custom_icon_renderer || m_icon_type) ? (f.x() + 44.0) : (f.x() + 16.0);
    const double text_y = f.y() + (f.height() - m_font_size) * 0.5;

    painter.draw_text(Point(text_x, text_y), m_label, text_col, m_font_size);
}

bool NavItem::handle_event(const Event& event) noexcept {
    if (!m_enabled) return false;

    if (event.type == EventType::PointerMove) {
        const Point p(event.pointer.x, event.pointer.y);
        const bool inside = frame().contains(p);
        if (m_hovered != inside) {
            m_hovered = inside;
            mark_needs_paint();
        }
        return inside;
    }

    if (event.type == EventType::PointerButtonPress) {
        if (event.pointer.button == MouseButton::Left) {
            const Point p(event.pointer.x, event.pointer.y);
            if (frame().contains(p)) {
                m_pressed = true;
                mark_needs_paint();
                return true;
            }
        }
        return false;
    }

    if (event.type == EventType::PointerButtonRelease) {
        if (event.pointer.button == MouseButton::Left && m_pressed) {
            m_pressed = false;
            mark_needs_paint();
            const Point p(event.pointer.x, event.pointer.y);
            if (frame().contains(p)) {
                if (m_on_click) {
                    m_on_click();
                }
                return true;
            }
        }
        return false;
    }

    return false;
}

} // namespace txui
