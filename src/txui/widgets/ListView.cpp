#include <txui/widgets/ListView.hpp>
#include <txui/input/Event.hpp>
#include <memory>
#include <chrono>

namespace txui {

ListView::ListView() noexcept {
    m_scroll_area = make_ref<ScrollArea>();
    m_scroll_area->set_allow_scroll_x(false);
    
    m_content_layout = make_ref<FlexLayout>();
    m_content_layout->set_direction(FlexDirection::Column);
    m_content_layout->set_main_axis_alignment(MainAxisAlignment::Start);
    m_content_layout->set_cross_axis_alignment(CrossAxisAlignment::Stretch);
    
    m_scroll_area->add_child(m_content_layout);
    add_child(m_scroll_area);
}

void ListView::add_item(Ref<Widget> item) noexcept {
    m_content_layout->add_child(item);
    mark_needs_measure();
}

void ListView::clear_items() noexcept {
    m_content_layout->remove_all_children();
    m_selected_index = -1;
    m_hover_index = -1;
    mark_needs_measure();
}

void ListView::set_selected_index(int32 index) noexcept {
    if (m_selected_index != index) {
        m_selected_index = index;
        ensure_visible(m_selected_index);
        mark_needs_paint();
        if (m_on_selected) {
            m_on_selected(m_selected_index);
        }
    }
}

void ListView::ensure_visible(int32 index) noexcept {
    if (index < 0 || m_content_layout->children().empty() || index >= static_cast<int32>(m_content_layout->children().size())) {
        return;
    }
    double item_top = 0.0;
    const auto& kids = m_content_layout->children();
    for (int32 i = 0; i < index; ++i) {
        item_top += kids[static_cast<size_t>(i)]->desired_size().height;
    }
    double item_h = kids[static_cast<size_t>(index)]->desired_size().height;
    if (item_h <= 0.0) item_h = 28.0;
    double item_bottom = item_top + item_h;
    double viewport_h = m_scroll_area->frame().height();
    if (viewport_h <= 0.0) viewport_h = frame().height();
    if (viewport_h > 0.0) {
        if (item_top < m_scroll_area->scroll_y()) {
            m_scroll_area->set_scroll_y(item_top);
        } else if (item_bottom > m_scroll_area->scroll_y() + viewport_h) {
            m_scroll_area->set_scroll_y(item_bottom - viewport_h);
        }
    }
}

Size ListView::measure_override(const Constraints& constraints) noexcept {
    m_scroll_area->measure(constraints);
    return constraints.constrain(m_scroll_area->desired_size());
}

void ListView::layout_override(const Rect& frame) noexcept {
    m_scroll_area->layout(Rect(frame.left(), frame.top(), frame.width(), frame.height()));
}

void ListView::paint_override(Painter& painter) const noexcept {
    // Background
    painter.fill_rect(frame(), Color(22, 24, 34, 255));

    painter.push_clip(frame());
    
    int32 i = 0;
    for (const auto& child : m_content_layout->children()) {
        Rect item_frame = child->frame();
        double draw_w = (item_frame.width() >= 999999.0) ? frame().width() : item_frame.width();
        
        if (i == m_selected_index) {
            // Selected row inset and custom color
            Rect inset_frame(item_frame.left() + 4.0, item_frame.top() + 2.0, 
                             std::max(0.0, draw_w - 8.0), std::max(0.0, item_frame.height() - 4.0));
            painter.fill_rounded_rect(inset_frame, 4.0, Color(45, 110, 225, 240));
        } else if (i == m_hover_index) {
            // Hover row inset and subtle color
            Rect inset_frame(item_frame.left() + 4.0, item_frame.top() + 2.0, 
                             std::max(0.0, draw_w - 8.0), std::max(0.0, item_frame.height() - 4.0));
            painter.fill_rounded_rect(inset_frame, 4.0, Color(255, 255, 255, 20));
        } else if (i % 2 == 1) {
            // Subtle alternating zebra row shading
            painter.fill_rect(Rect(item_frame.left(), item_frame.top(), draw_w, item_frame.height()), Color(255, 255, 255, 6));
        }
        
        // Subtle row bottom divider line
        painter.fill_rect(Rect(item_frame.left(), item_frame.bottom() - 1.0, draw_w, 1.0), Color(45, 48, 62, 120));
        i++;
    }
    
    m_scroll_area->paint(painter);
    
    painter.pop_clip();
}

bool ListView::handle_event(const Event& event) noexcept {
    if (event.type == EventType::PointerMove) {
        Point position(event.pointer.x, event.pointer.y);
        if (frame().contains(position)) {
            int32 i = 0;
            int32 found = -1;
            for (const auto& child : m_content_layout->children()) {
                if (child->frame().contains(position)) {
                    found = i;
                    break;
                }
                i++;
            }
            if (m_hover_index != found) {
                m_hover_index = found;
                mark_needs_paint();
            }
        } else {
            if (m_hover_index != -1) {
                m_hover_index = -1;
                mark_needs_paint();
            }
        }
    } else if (event.type == EventType::PointerButtonPress) {
        Point position(event.pointer.x, event.pointer.y);
        if (frame().contains(position)) {
            int32 i = 0;
            for (const auto& child : m_content_layout->children()) {
                if (child->frame().contains(position)) {
                    set_selected_index(i);
                    // Handle double click logic? Since txui doesn't have double-click event yet, we could mock it.
                    // For MVP, maybe we'll use a specific event or time-based logic.
                    // For now, if it's already selected and clicked again, treat as double click!
                    auto now = std::chrono::steady_clock::now();
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();
                    
                    if (i == m_last_clicked_index && diff < 500) {
                        if (m_on_double_clicked) m_on_double_clicked(i);
                        m_last_clicked_index = -1;
                    } else {
                        m_last_clicked_index = i;
                        m_last_click_time = now;
                    }
                    return true;
                }
                i++;
            }
        }
    }

    return m_scroll_area->handle_event(event);
}

} // namespace txui
