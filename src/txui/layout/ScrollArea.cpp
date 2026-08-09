#include <txui/layout/ScrollArea.hpp>
#include <algorithm>
#include <cmath>

namespace txui {

void ScrollArea::set_scroll_y(double y) noexcept {
    double new_y = std::clamp(y, 0.0, m_max_scroll_y);
    if (m_scroll_y != new_y) {
        m_scroll_y = new_y;
        mark_needs_paint();
    }
}

void ScrollArea::set_scroll_x(double x) noexcept {
    double new_x = std::clamp(x, 0.0, m_max_scroll_x);
    if (m_scroll_x != new_x) {
        m_scroll_x = new_x;
        mark_needs_paint();
    }
}

Size ScrollArea::measure_override(const Constraints& constraints) noexcept {
    if (children().empty()) {
        return constraints.constrain(Size(0, 0));
    }

    // ScrollArea provides infinite space to its children for measurement
    Constraints child_constraints(0, INF, 0, INF);
    
    // Pass width constraints if we only want vertical scrolling, etc.
    // But for a generic ScrollArea, we provide infinite bounds and see what the child wants.
    // For Tinexus files, we might want to constrain width to max_width if we only scroll vertically.
    // We'll provide max_width constraint but allow infinite width, but recommend max_width.
    child_constraints.max_width = INF;
    child_constraints.max_height = INF;

    auto& child = children().front();
    child->measure(child_constraints);

    // Our own desired size is bounded by the constraints provided by our parent
    double desired_w = std::min(constraints.max_width, child->desired_size().width);
    double desired_h = std::min(constraints.max_height, child->desired_size().height);
    
    // Compute max scroll
    m_max_scroll_x = std::max(0.0, child->desired_size().width - desired_w);
    m_max_scroll_y = std::max(0.0, child->desired_size().height - desired_h);

    return constraints.constrain(Size(desired_w, desired_h));
}

void ScrollArea::layout_override(const Rect& frame) noexcept {
    if (children().empty()) return;

    auto& child = children().front();
    
    // Update max scroll limits based on actual frame size
    m_max_scroll_x = std::max(0.0, child->desired_size().width - frame.width());
    m_max_scroll_y = std::max(0.0, child->desired_size().height - frame.height());
    
    // Clamp current scroll offsets
    m_scroll_x = std::clamp(m_scroll_x, 0.0, m_max_scroll_x);
    m_scroll_y = std::clamp(m_scroll_y, 0.0, m_max_scroll_y);

    // Position child with negative offset so it shifts up/left when scrolled down/right
    Rect child_frame(
        frame.left() - m_scroll_x,
        frame.top() - m_scroll_y,
        child->desired_size().width,
        child->desired_size().height
    );

    child->layout(child_frame);
}

void ScrollArea::paint_override(Painter& painter) const noexcept {
    if (children().empty()) return;

    painter.push_clip(frame());
    children().front()->paint(painter);
    painter.pop_clip();
}

bool ScrollArea::handle_event(const Event& event) noexcept {
    // Let child handle first
    if (Widget::handle_event(event)) {
        return true;
    }

    if (const auto* wheel = std::get_if<PointerScrollEvent>(&event)) {
        if (frame().contains(wheel->position)) {
            // Scroll vertical
            if (wheel->dy != 0.0 && m_max_scroll_y > 0.0) {
                set_scroll_y(m_scroll_y + wheel->dy * 30.0);
                return true;
            }
            // Scroll horizontal
            if (wheel->dx != 0.0 && m_max_scroll_x > 0.0) {
                set_scroll_x(m_scroll_x + wheel->dx * 30.0);
                return true;
            }
        }
    }

    return false;
}

} // namespace txui
