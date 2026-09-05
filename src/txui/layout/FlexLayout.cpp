#include <txui/layout/FlexLayout.hpp>
#include <vector>

namespace txui {

Size FlexLayout::measure_override(const Constraints& constraints) noexcept {
    if (children().empty()) {
        return constraints.constrain(Size(0.0, 0.0));
    }

    const bool is_row = m_direction == FlexDirection::Row;
    const float64 max_main_size = is_row ? constraints.max_width : constraints.max_height;
    const bool can_flex = max_main_size < INF;

    float64 allocated_main_size = 0.0;
    float64 max_cross_size = 0.0;
    int32 total_flex = 0;

    // Step 1: Measure non-flexible children and accumulate flex factors
    for (auto& child : children()) {
        auto* flex_item = dynamic_cast<FlexItem*>(child.get());
        int32 flex_val = (flex_item != nullptr) ? flex_item->flex() : 0;
        
        if (flex_val > 0 && can_flex) {
            total_flex += flex_val;
        } else {
            // Non-flexible child
            Constraints child_constraints;
            if (is_row) {
                child_constraints = Constraints(
                    0.0, INF,
                    m_cross_axis_alignment == CrossAxisAlignment::Stretch ? constraints.max_height : 0.0,
                    constraints.max_height
                );
            } else {
                child_constraints = Constraints(
                    m_cross_axis_alignment == CrossAxisAlignment::Stretch ? constraints.max_width : 0.0,
                    constraints.max_width,
                    0.0, INF
                );
            }
            
            child->measure(child_constraints);
            
            allocated_main_size += is_row ? child->desired_size().width : child->desired_size().height;
            max_cross_size = std::max(max_cross_size, is_row ? child->desired_size().height : child->desired_size().width);
        }
    }

    // Step 2: Measure flexible children
    if (total_flex > 0 && can_flex) {
        float64 free_space = std::max(0.0, max_main_size - allocated_main_size);
        float64 space_per_flex = free_space / static_cast<float64>(total_flex);

        for (auto& child : children()) {
            auto* flex_item = dynamic_cast<FlexItem*>(child.get());
            if (flex_item != nullptr && flex_item->flex() > 0) {
                float64 child_main_space = space_per_flex * static_cast<float64>(flex_item->flex());
                
                Constraints child_constraints;
                if (is_row) {
                    float64 min_w = flex_item->expanded() ? child_main_space : 0.0;
                    child_constraints = Constraints(
                        min_w, child_main_space,
                        m_cross_axis_alignment == CrossAxisAlignment::Stretch ? constraints.max_height : 0.0,
                        constraints.max_height
                    );
                } else {
                    float64 min_h = flex_item->expanded() ? child_main_space : 0.0;
                    child_constraints = Constraints(
                        m_cross_axis_alignment == CrossAxisAlignment::Stretch ? constraints.max_width : 0.0,
                        constraints.max_width,
                        min_h, child_main_space
                    );
                }

                child->measure(child_constraints);

                allocated_main_size += is_row ? child->desired_size().width : child->desired_size().height;
                max_cross_size = std::max(max_cross_size, is_row ? child->desired_size().height : child->desired_size().width);
            }
        }
    }

    float64 final_width = is_row ? allocated_main_size : max_cross_size;
    float64 final_height = is_row ? max_cross_size : allocated_main_size;

    return constraints.constrain(Size(final_width, final_height));
}

void FlexLayout::layout_override(const Rect& frame) noexcept {
    if (children().empty()) return;

    const bool is_row = m_direction == FlexDirection::Row;
    const float64 main_size = is_row ? frame.width() : frame.height();
    const float64 cross_size = is_row ? frame.height() : frame.width();

    float64 total_children_main_size = 0.0;
    for (auto& child : children()) {
        total_children_main_size += is_row ? child->desired_size().width : child->desired_size().height;
    }

    float64 remaining_space = std::max(0.0, main_size - total_children_main_size);
    float64 initial_offset = 0.0;
    float64 space_between = 0.0;

    size_t count = children().size();

    switch (m_main_axis_alignment) {
        case MainAxisAlignment::Start:
            break;
        case MainAxisAlignment::End:
            initial_offset = remaining_space;
            break;
        case MainAxisAlignment::Center:
            initial_offset = remaining_space / 2.0;
            break;
        case MainAxisAlignment::SpaceBetween:
            if (count > 1) space_between = remaining_space / static_cast<float64>(count - 1);
            break;
        case MainAxisAlignment::SpaceAround:
            if (count > 0) {
                space_between = remaining_space / static_cast<float64>(count);
                initial_offset = space_between / 2.0;
            }
            break;
        case MainAxisAlignment::SpaceEvenly:
            if (count > 0) {
                space_between = remaining_space / static_cast<float64>(count + 1);
                initial_offset = space_between;
            }
            break;
    }

    float64 current_main_offset = is_row ? frame.left() + initial_offset : frame.top() + initial_offset;
    const float64 cross_offset = is_row ? frame.top() : frame.left();

    for (auto& child : children()) {
        float64 child_main_size = is_row ? child->desired_size().width : child->desired_size().height;
        float64 child_cross_size = is_row ? child->desired_size().height : child->desired_size().width;
        
        float64 child_cross_offset = cross_offset;

        switch (m_cross_axis_alignment) {
            case CrossAxisAlignment::Start:
                break;
            case CrossAxisAlignment::Stretch:
                child_cross_size = cross_size;
                break;
            case CrossAxisAlignment::End:
                child_cross_offset += (cross_size - child_cross_size);
                break;
            case CrossAxisAlignment::Center:
                child_cross_offset += (cross_size - child_cross_size) / 2.0;
                break;
        }

        if (is_row) {
            child->layout(Rect(current_main_offset, child_cross_offset, child_main_size, child_cross_size));
        } else {
            child->layout(Rect(child_cross_offset, current_main_offset, child_cross_size, child_main_size));
        }

        current_main_offset += child_main_size + space_between;
    }
}

} // namespace txui
