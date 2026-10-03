#include <txui/widgets/Slider.hpp>
#include <txui/input/Event.hpp>
#include <algorithm>

namespace txui {

Slider::Slider(double min_val, double max_val, double initial_val) noexcept
    : m_min(min_val), m_max(std::max(min_val + 0.0001, max_val)), m_value(std::clamp(initial_val, min_val, max_val)) {}

void Slider::set_range(double min_val, double max_val) noexcept {
    m_min = min_val;
    m_max = std::max(min_val + 0.0001, max_val);
    m_value = std::clamp(m_value, m_min, m_max);
    mark_needs_paint();
}

void Slider::set_value(double val) noexcept {
    double clamped = std::clamp(val, m_min, m_max);
    if (m_value != clamped) {
        m_value = clamped;
        mark_needs_paint();
        if (m_on_value_changed) {
            m_on_value_changed(m_value);
        }
    }
}

void Slider::set_enabled(bool enabled) noexcept {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!m_enabled) {
            m_dragging = false;
            m_hovered = false;
        }
        mark_needs_paint();
    }
}

void Slider::set_active_color(Color color) noexcept {
    if (m_active_color != color) {
        m_active_color = color;
        mark_needs_paint();
    }
}

void Slider::set_inactive_color(Color color) noexcept {
    if (m_inactive_color != color) {
        m_inactive_color = color;
        mark_needs_paint();
    }
}

Size Slider::measure_override(const Constraints& constraints) noexcept {
    return constraints.constrain(Size(140.0, 28.0));
}

void Slider::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();
    const double thumb_r = 8.0;
    const double usable_w = std::max(0.0, f.width() - (thumb_r * 2.0));
    const double t = (m_max > m_min) ? std::clamp((m_value - m_min) / (m_max - m_min), 0.0, 1.0) : 0.0;

    const double track_h = 4.0;
    const double track_y = f.y() + (f.height() - track_h) * 0.5;
    const double track_x = f.x() + thumb_r;

    // 1. Inactive track trough
    painter.fill_rounded_rect(Rect(track_x, track_y, usable_w, track_h), 2.0, m_inactive_color);
    painter.fill_rounded_rect(Rect(track_x, track_y + track_h - 1.0, usable_w, 1.0), 1.0, Color(0, 0, 0, 60));

    // 2. Active filled track
    const double fill_w = t * usable_w;
    if (fill_w > 0.0) {
        painter.fill_rounded_rect(Rect(track_x, track_y, fill_w, track_h), 2.0, m_active_color);
    }

    // 3. Draggable Thumb
    const double thumb_x = track_x + fill_w;
    const double thumb_y = f.y() + f.height() * 0.5;

    // Thumb shadow
    painter.fill_circle(Point(thumb_x, thumb_y + 1.0), thumb_r + 1.0, Color(0, 0, 0, 70));

    // Thumb body
    const Color thumb_body_col = m_enabled ? Color(255, 255, 255, 255) : Color(180, 185, 195, 255);
    painter.fill_circle(Point(thumb_x, thumb_y), thumb_r, thumb_body_col);

    // Thumb inner accent dot when dragging or hovering
    if (m_dragging || m_hovered) {
        painter.fill_circle(Point(thumb_x, thumb_y), 3.0, m_active_color);
    }
}

bool Slider::handle_event(const Event& event) noexcept {
    if (!m_enabled) return false;

    const double thumb_r = 8.0;
    const double usable_w = std::max(1.0, frame().width() - (thumb_r * 2.0));
    const double track_x = frame().x() + thumb_r;

    if (event.type == EventType::PointerMove) {
        Point p(event.pointer.x, event.pointer.y);
        if (m_dragging) {
            double t = std::clamp((p.x - track_x) / usable_w, 0.0, 1.0);
            set_value(m_min + t * (m_max - m_min));
            return true;
        }
        bool inside = frame().contains(p);
        if (m_hovered != inside) {
            m_hovered = inside;
            mark_needs_paint();
        }
        return inside;
    } else if (event.type == EventType::PointerButtonPress) {
        Point p(event.pointer.x, event.pointer.y);
        if (frame().contains(p) && event.pointer.button == MouseButton::Left) {
            m_dragging = true;
            double t = std::clamp((p.x - track_x) / usable_w, 0.0, 1.0);
            set_value(m_min + t * (m_max - m_min));
            return true;
        }
    } else if (event.type == EventType::PointerButtonRelease) {
        if (m_dragging && event.pointer.button == MouseButton::Left) {
            m_dragging = false;
            mark_needs_paint();
            return true;
        }
    } else if (event.type == EventType::PointerLeave) {
        if (m_hovered && !m_dragging) {
            m_hovered = false;
            mark_needs_paint();
        }
    }
    return false;
}

} // namespace txui
