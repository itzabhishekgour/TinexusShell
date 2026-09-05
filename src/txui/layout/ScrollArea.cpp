#include <txui/layout/ScrollArea.hpp>
#include <algorithm>
#include <cmath>

namespace txui {

void ScrollArea::set_scroll_y(double y) noexcept {
    double new_y = std::clamp(y, 0.0, m_max_scroll_y);
    if (m_scroll_y != new_y) {
        m_scroll_y = new_y;
        mark_needs_layout(); // must reposition child, not just repaint
    }
}

void ScrollArea::set_scroll_x(double x) noexcept {
    double new_x = std::clamp(x, 0.0, m_max_scroll_x);
    if (m_scroll_x != new_x) {
        m_scroll_x = new_x;
        mark_needs_layout(); // must reposition child, not just repaint
    }
}

Size ScrollArea::measure_override(const Constraints& constraints) noexcept {
    if (children().empty()) {
        return constraints.constrain(Size(0, 0));
    }

    // Pass infinite constraints only if scrolling is allowed in that direction
    Constraints child_constraints(0, INF, 0, INF);
    child_constraints.max_width = m_allow_scroll_x ? INF : constraints.max_width;
    child_constraints.max_height = m_allow_scroll_y ? INF : constraints.max_height;

    auto& child = children().front();
    child->measure(child_constraints);

    // Our own desired size is bounded by the constraints provided by our parent
    double desired_w = std::min(constraints.max_width, child->desired_size().width);
    double desired_h = std::min(constraints.max_height, child->desired_size().height);
    
    // Compute max scroll
    m_max_scroll_x = m_allow_scroll_x ? std::max(0.0, child->desired_size().width - desired_w) : 0.0;
    m_max_scroll_y = m_allow_scroll_y ? std::max(0.0, child->desired_size().height - desired_h) : 0.0;

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
    
    // Draw subtle macOS-style scrollbar pill if content overflows vertically
    if (m_max_scroll_y > 0.0) {
        double track_h = frame().height();
        double thumb_h = std::max(24.0, track_h * (track_h / (track_h + m_max_scroll_y)));
        double thumb_y = frame().top() + (m_scroll_y / m_max_scroll_y) * (track_h - thumb_h);
        Rect thumb_rect(frame().right() - 5.0, thumb_y, 3.0, thumb_h);
        painter.fill_rounded_rect(thumb_rect, 1.5, Color(255, 255, 255, 70));
    }

    // Draw subtle scrollbar pill if content overflows horizontally
    if (m_max_scroll_x > 0.0) {
        double track_w = frame().width();
        double thumb_w = std::max(24.0, track_w * (track_w / (track_w + m_max_scroll_x)));
        double thumb_x = frame().left() + (m_scroll_x / m_max_scroll_x) * (track_w - thumb_w);
        Rect thumb_rect(thumb_x, frame().bottom() - 5.0, thumb_w, 3.0);
        painter.fill_rounded_rect(thumb_rect, 1.5, Color(255, 255, 255, 70));
    }

    painter.pop_clip();
}

bool ScrollArea::handle_event(const Event& event) noexcept {
    // Let child handle first
    if (Widget::handle_event(event)) {
        return true;
    }

    if (event.type == EventType::PointerScroll) {
        Point position(event.pointer.x, event.pointer.y);
        bool inside = frame().contains(position) || (event.pointer.x == 0.0 && event.pointer.y == 0.0);
        if (inside) {
            // Map vertical scroll to horizontal scroll if vertical scrolling is disabled
            // This enables horizontal mouse-wheel scrolling across columns when the mouse is over the background
            double dy = event.pointer.scroll_delta_y;
            double dx = event.pointer.scroll_delta_x;
            
            if (dy != 0.0 && !m_allow_scroll_y && m_allow_scroll_x) {
                dx += dy;
                dy = 0.0;
            }
            
            // Scroll vertical
            if (dy != 0.0 && m_allow_scroll_y && m_max_scroll_y > 0.0) {
                double step = (std::abs(dy) < 5.0) ? dy * 30.0 : dy * 3.0;
                set_scroll_y(m_scroll_y + step);
                return true;
            }
            // Scroll horizontal
            if (dx != 0.0 && m_allow_scroll_x && m_max_scroll_x > 0.0) {
                double step = (std::abs(dx) < 5.0) ? dx * 30.0 : dx * 3.0;
                set_scroll_x(m_scroll_x + step);
                return true;
            }
        }
    }

    return false;
}

} // namespace txui
