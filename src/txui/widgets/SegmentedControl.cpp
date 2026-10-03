#include <txui/widgets/SegmentedControl.hpp>
#include <txui/input/Event.hpp>
#include <algorithm>

namespace txui {

SegmentedControl::SegmentedControl(std::vector<std::string> segments, size_t default_index) noexcept
    : m_segments(std::move(segments)), m_selected_index(default_index) {
    if (m_selected_index >= m_segments.size() && !m_segments.empty()) {
        m_selected_index = 0;
    }
}

void SegmentedControl::set_segments(std::vector<std::string> segments) noexcept {
    m_segments = std::move(segments);
    if (m_selected_index >= m_segments.size() && !m_segments.empty()) {
        m_selected_index = 0;
    }
    m_hovered_index = -1;
    mark_needs_measure();
    mark_needs_paint();
}

void SegmentedControl::set_selected_index(size_t index) noexcept {
    if (index < m_segments.size() && m_selected_index != index) {
        m_selected_index = index;
        mark_needs_paint();
        if (m_on_segment_selected) {
            m_on_segment_selected(m_selected_index);
        }
    }
}

void SegmentedControl::set_font_size(double size) noexcept {
    if (m_font_size != size) {
        m_font_size = size;
        mark_needs_measure();
        mark_needs_paint();
    }
}

void SegmentedControl::set_corner_radius(double radius) noexcept {
    if (m_corner_radius != radius) {
        m_corner_radius = radius;
        mark_needs_paint();
    }
}

Rect SegmentedControl::segment_rect(size_t index) const noexcept {
    if (m_segments.empty() || index >= m_segments.size()) {
        return Rect(0.0, 0.0, 0.0, 0.0);
    }

    const Rect f = frame();
    const double pad = 3.0;
    const double usable_w = std::max(0.0, f.width() - (pad * 2.0));
    const double usable_h = std::max(0.0, f.height() - (pad * 2.0));
    const double seg_w = usable_w / static_cast<double>(m_segments.size());

    return Rect(f.x() + pad + static_cast<double>(index) * seg_w,
                f.y() + pad,
                seg_w, usable_h);
}

Size SegmentedControl::measure_override(const Constraints& constraints) noexcept {
    if (m_segments.empty()) {
        return constraints.constrain(Size(40.0, 36.0));
    }

    double max_seg_w = 40.0;
    for (const auto& label : m_segments) {
        const auto extents = FontMetrics::measure(label, m_font_size);
        max_seg_w = std::max(max_seg_w, extents.width + 28.0); // 14px padding per side
    }

    const double total_w = max_seg_w * static_cast<double>(m_segments.size()) + 6.0;
    const double total_h = std::max(36.0, m_font_size + 20.0);
    return constraints.constrain(Size(total_w, total_h));
}

void SegmentedControl::paint_override(Painter& painter) const noexcept {
    const Rect f = frame();
    if (m_segments.empty()) return;

    // 1. Container track background
    painter.fill_rounded_rect(f, m_corner_radius, Color(22, 25, 34, 230));

    // Outer subtle border
    painter.fill_rounded_rect(Rect(f.x(), f.y(), f.width(), 1.0), 1.0, Color(255, 255, 255, 20));
    painter.fill_rounded_rect(Rect(f.x(), f.y() + f.height() - 1.0, f.width(), 1.0), 1.0, Color(0, 0, 0, 50));

    const double inner_radius = std::max(2.0, m_corner_radius - 2.0);

    // 2. Segments rendering
    for (size_t i = 0; i < m_segments.size(); ++i) {
        const Rect r = segment_rect(i);
        const bool is_active = (i == m_selected_index);
        const bool is_hover = (static_cast<int>(i) == m_hovered_index);

        if (is_active) {
            // Elevated active segment pill
            painter.fill_rounded_rect(r, inner_radius, Color(58, 64, 82, 255));
            // Subtle 1px top highlight on active segment
            painter.fill_rounded_rect(Rect(r.x(), r.y(), r.width(), 1.0), 1.0, Color(255, 255, 255, 45));
        } else if (is_hover) {
            // Hover highlight
            painter.fill_rounded_rect(r, inner_radius, Color(255, 255, 255, 14));
        }

        // Segment label text
        const auto& text = m_segments[i];
        const auto extents = FontMetrics::measure(text, m_font_size);
        const double tx = r.x() + (r.width() - extents.width) * 0.5;
        const double ty = r.y() + (r.height() - extents.height) * 0.5;

        const Color text_col = is_active
            ? Color(255, 255, 255, 255)
            : (is_hover ? Color(210, 215, 225, 240) : Color(155, 160, 175, 220));

        painter.draw_text(Point(tx, ty), text, text_col, m_font_size, is_active);
    }
}

bool SegmentedControl::handle_event(const Event& event) noexcept {
    if (event.type == EventType::PointerMove) {
        Point p(event.pointer.x, event.pointer.y);
        int new_hover = -1;
        if (frame().contains(p)) {
            for (size_t i = 0; i < m_segments.size(); ++i) {
                if (segment_rect(i).contains(p)) {
                    new_hover = static_cast<int>(i);
                    break;
                }
            }
        }
        if (m_hovered_index != new_hover) {
            m_hovered_index = new_hover;
            mark_needs_paint();
        }
        return frame().contains(p);
    } else if (event.type == EventType::PointerButtonPress) {
        Point p(event.pointer.x, event.pointer.y);
        if (frame().contains(p) && event.pointer.button == MouseButton::Left) {
            for (size_t i = 0; i < m_segments.size(); ++i) {
                if (segment_rect(i).contains(p)) {
                    set_selected_index(i);
                    return true;
                }
            }
        }
    } else if (event.type == EventType::PointerLeave) {
        if (m_hovered_index != -1) {
            m_hovered_index = -1;
            mark_needs_paint();
        }
    }
    return false;
}

} // namespace txui
