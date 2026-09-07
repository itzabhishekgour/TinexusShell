#include <txui/widgets/ToggleSwitch.hpp>
#include <txui/input/Event.hpp>
#include <algorithm>

namespace txui {

ToggleSwitch::ToggleSwitch(bool checked, std::function<void(bool)> on_toggled) noexcept
    : m_checked(checked), m_on_toggled(std::move(on_toggled)) {}

void ToggleSwitch::set_checked(bool checked) noexcept {
    if (m_checked != checked) {
        m_checked = checked;
        mark_needs_paint();
        if (m_on_toggled) {
            m_on_toggled(m_checked);
        }
    }
}

void ToggleSwitch::set_enabled(bool enabled) noexcept {
    if (m_enabled != enabled) {
        m_enabled = enabled;
        if (!m_enabled) {
            m_hovered = false;
        }
        mark_needs_paint();
    }
}

void ToggleSwitch::set_active_color(Color color) noexcept {
    if (m_active_color != color) {
        m_active_color = color;
        mark_needs_paint();
    }
}

void ToggleSwitch::set_inactive_color(Color color) noexcept {
    if (m_inactive_color != color) {
        m_inactive_color = color;
        mark_needs_paint();
    }
}

Size ToggleSwitch::measure_override(const Constraints& constraints) noexcept {
    return constraints.constrain(Size(46.0, 26.0));
}

void ToggleSwitch::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();
    const double radius = f.height() * 0.5;

    // Track color selection
    Color track_col;
    if (!m_enabled) {
        track_col = Color(40, 42, 50, 180);
    } else if (m_checked) {
        track_col = m_hovered
            ? Color(static_cast<uint8>(std::min(255, static_cast<int>(m_active_color.r()) + 20)),
                    static_cast<uint8>(std::min(255, static_cast<int>(m_active_color.g()) + 20)),
                    static_cast<uint8>(std::min(255, static_cast<int>(m_active_color.b()) + 20)),
                    m_active_color.a())
            : m_active_color;
    } else {
        track_col = m_hovered ? Color(60, 65, 80, 240) : m_inactive_color;
    }

    // 1. Draw Pill Track
    painter.fill_rounded_rect(f, radius, track_col);

    // Track highlight / inner border
    painter.fill_rounded_rect(Rect(f.x(), f.y(), f.width(), 1.0), 1.0, Color(255, 255, 255, m_checked ? 60 : 25));
    painter.fill_rounded_rect(Rect(f.x(), f.y() + f.height() - 1.0, f.width(), 1.0), 1.0, Color(0, 0, 0, 50));

    // 2. Sliding Thumb
    const double inset = 3.0;
    const double thumb_d = std::max(4.0, f.height() - (inset * 2.0));
    const double thumb_r = thumb_d * 0.5;

    const double thumb_x = m_checked
        ? (f.x() + f.width() - inset - thumb_r)
        : (f.x() + inset + thumb_r);
    const double thumb_y = f.y() + (f.height() * 0.5);

    // Thumb shadow
    painter.fill_circle(Point(thumb_x, thumb_y + 1.0), thumb_r, Color(0, 0, 0, 80));

    // Thumb circle
    const Color thumb_col = m_enabled ? Color(255, 255, 255, 255) : Color(180, 185, 195, 255);
    painter.fill_circle(Point(thumb_x, thumb_y), thumb_r, thumb_col);
}

bool ToggleSwitch::handle_event(const Event& event) noexcept {
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
            set_checked(!m_checked);
            return true;
        }
    } else if (event.type == EventType::PointerLeave) {
        if (m_hovered) {
            m_hovered = false;
            mark_needs_paint();
        }
    }
    return false;
}

} // namespace txui
