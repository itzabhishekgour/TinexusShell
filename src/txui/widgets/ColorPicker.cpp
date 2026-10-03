#include <txui/widgets/ColorPicker.hpp>
#include <txui/input/Event.hpp>
#include <algorithm>
#include <cmath>

namespace txui {

ColorPicker::ColorPicker(std::vector<Color> colors, size_t selected_index,
                         std::function<void(size_t, Color)> on_selected) noexcept
    : m_colors(std::move(colors)),
      m_selected_index(selected_index),
      m_on_color_selected(std::move(on_selected)) {}

void ColorPicker::set_colors(std::vector<Color> colors) noexcept {
    m_colors = std::move(colors);
    if (m_selected_index >= m_colors.size() && !m_colors.empty()) {
        m_selected_index = 0;
    }
    mark_needs_measure();
    mark_needs_paint();
}

void ColorPicker::set_selected_index(size_t index) noexcept {
    if (index < m_colors.size() && m_selected_index != index) {
        m_selected_index = index;
        mark_needs_paint();
    }
}

Color ColorPicker::selected_color() const noexcept {
    if (m_selected_index < m_colors.size()) {
        return m_colors[m_selected_index];
    }
    return Color(0, 195, 255, 255);
}

void ColorPicker::set_swatch_radius(double radius) noexcept {
    if (m_swatch_radius != radius) {
        m_swatch_radius = radius;
        mark_needs_measure();
        mark_needs_paint();
    }
}

void ColorPicker::set_spacing(double spacing) noexcept {
    if (m_spacing != spacing) {
        m_spacing = spacing;
        mark_needs_measure();
        mark_needs_paint();
    }
}

Size ColorPicker::measure_override(const Constraints& constraints) noexcept {
    const size_t count = m_colors.size();
    const double diam = m_swatch_radius * 2.0;
    const double cnt_d = static_cast<double>(count);
    const double w = (count > 0) ? (cnt_d * diam + (cnt_d - 1.0) * m_spacing + 16.0) : 40.0;
    const double h = diam + 20.0;
    return constraints.constrain(Size(w, h));
}

int ColorPicker::hit_test_index(double x, double y) const noexcept {
    const Rect f = frame();
    const double diam = m_swatch_radius * 2.0;
    const double cy = f.y() + f.height() * 0.5;

    for (size_t i = 0; i < m_colors.size(); ++i) {
        const double cx = f.x() + 8.0 + m_swatch_radius + static_cast<double>(i) * (diam + m_spacing);
        const double dx = x - cx;
        const double dy = y - cy;
        const double dist_sq = dx * dx + dy * dy;
        const double hit_r = m_swatch_radius + 6.0;
        if (dist_sq <= hit_r * hit_r) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void ColorPicker::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();
    if (m_colors.empty() || f.width() <= 0.0 || f.height() <= 0.0) return;

    const double diam = m_swatch_radius * 2.0;
    const double cy = f.y() + f.height() * 0.5;

    for (size_t i = 0; i < m_colors.size(); ++i) {
        const double cx = f.x() + 8.0 + m_swatch_radius + static_cast<double>(i) * (diam + m_spacing);
        const Point center(cx, cy);
        const Color& col = m_colors[i];

        // Selection ring
        if (m_selected_index == i) {
            // Concentric outer halo indicator
            painter.draw_circle(center, m_swatch_radius + 4.5, 2.0, col);
            // Inner gap
            painter.draw_circle(center, m_swatch_radius + 1.5, 1.0, Color(20, 22, 30, 200));
        } else if (m_hovered_index == static_cast<int>(i)) {
            // Hover halo
            painter.draw_circle(center, m_swatch_radius + 3.5, 1.5, Color(255, 255, 255, 100));
        }

        // Swatch body
        painter.fill_circle(center, m_swatch_radius, col);
    }
}

bool ColorPicker::handle_event(const Event& event) noexcept {
    if (event.type == EventType::PointerMove) {
        const int idx = hit_test_index(event.pointer.x, event.pointer.y);
        if (m_hovered_index != idx) {
            m_hovered_index = idx;
            mark_needs_paint();
        }
        return idx != -1;
    }

    if (event.type == EventType::PointerButtonPress) {
        if (event.pointer.button == MouseButton::Left) {
            const int idx = hit_test_index(event.pointer.x, event.pointer.y);
            if (idx != -1) {
                m_pressed = true;
                return true;
            }
        }
        return false;
    }

    if (event.type == EventType::PointerButtonRelease) {
        if (event.pointer.button == MouseButton::Left && m_pressed) {
            m_pressed = false;
            const int idx = hit_test_index(event.pointer.x, event.pointer.y);
            if (idx != -1 && static_cast<size_t>(idx) < m_colors.size()) {
                m_selected_index = static_cast<size_t>(idx);
                mark_needs_paint();
                if (m_on_color_selected) {
                    m_on_color_selected(m_selected_index, m_colors[m_selected_index]);
                }
                return true;
            }
        }
        return false;
    }

    return false;
}

} // namespace txui
