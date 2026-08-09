#include <txui/widgets/ListView.hpp>

namespace txui {

ListView::ListView() noexcept {
    m_scroll_area = std::make_shared<ScrollArea>();
    m_content_layout = std::make_shared<FlexLayout>(FlexDirection::Column, FlexAlignment::Start, CrossAxisAlignment::Stretch);
    
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
        mark_needs_paint();
        if (m_on_selected) {
            m_on_selected(m_selected_index);
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
    painter.fill_rect(frame(), Color(40, 40, 40, 255)); // macOS Dark mode list background

    // Paint hover/selection highlights behind the items
    // The items themselves are in the scroll area, so their frame is offset by scroll.
    // We need to apply the scroll offset clip, but wait, it's easier if ListView handles the selection drawing
    // OR the items themselves could handle it. Since items are just Widgets, ListView can paint the selection rectangles.
    
    painter.push_clip(frame());
    
    int32 i = 0;
    for (const auto& child : m_content_layout->children()) {
        Rect item_frame = child->frame();
        // The child frame is relative to m_content_layout, but since m_scroll_area is root child, 
        // its actual screen position is what child->frame() contains after layout!
        
        if (i == m_selected_index) {
            painter.fill_rounded_rect(item_frame, 4.0, Color(0, 100, 255, 255)); // Mac Blue
        } else if (i == m_hover_index) {
            painter.fill_rounded_rect(item_frame, 4.0, Color(255, 255, 255, 30));
        }
        i++;
    }
    
    m_scroll_area->paint(painter);
    
    painter.pop_clip();
}

bool ListView::handle_event(const Event& event) noexcept {
    if (const auto* ptr = std::get_if<PointerMoveEvent>(&event)) {
        if (frame().contains(ptr->position)) {
            int32 i = 0;
            int32 found = -1;
            for (const auto& child : m_content_layout->children()) {
                if (child->frame().contains(ptr->position)) {
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
    } else if (const auto* click = std::get_if<PointerButtonEvent>(&event)) {
        if (click->state == ButtonState::Pressed && frame().contains(click->position)) {
            int32 i = 0;
            for (const auto& child : m_content_layout->children()) {
                if (child->frame().contains(click->position)) {
                    set_selected_index(i);
                    // Handle double click logic? Since txui doesn't have double-click event yet, we could mock it.
                    // For MVP, maybe we'll use a specific event or time-based logic.
                    // For now, if it's already selected and clicked again, treat as double click!
                    // Not ideal, but works for MVP navigation.
                    static int32 last_clicked = -1;
                    static auto last_time = std::chrono::steady_clock::now();
                    auto now = std::chrono::steady_clock::now();
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time).count();
                    
                    if (i == last_clicked && diff < 500) {
                        if (m_on_double_clicked) m_on_double_clicked(i);
                        last_clicked = -1;
                    } else {
                        last_clicked = i;
                        last_time = now;
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
