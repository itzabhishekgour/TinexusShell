#include "ColumnWidget.hpp"
#include <txui/widgets/Icon.hpp>
#include <txui/widgets/Label.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/layout/Padding.hpp>

namespace tinexus::files::ui {

ColumnWidget::ColumnWidget(const ColumnLevel& level, size_t col_index, bool is_last) 
    : m_model(&level), m_col_index(col_index), m_is_last_column(is_last) {
    m_list_view = txui::make_ref<txui::ListView>();
    
    m_list_view->set_on_selected([this](txui::int32 index) {
        if (m_on_item_selected && index >= 0) {
            m_on_item_selected(m_col_index, static_cast<size_t>(index));
        }
    });

    m_list_view->set_on_double_clicked([this](txui::int32 index) {
        if (m_on_item_double_clicked && index >= 0) {
            m_on_item_double_clicked(m_col_index, static_cast<size_t>(index));
        }
    });

    add_child(m_list_view);
    refresh(level);
}

void ColumnWidget::refresh(const ColumnLevel& level) noexcept {
    m_model = &level;
    m_list_view->clear_items();
    
    for (const auto& item : m_model->items) {
        auto row = txui::make_ref<txui::FlexLayout>();
        row->set_direction(txui::FlexDirection::Row);
        row->set_main_axis_alignment(txui::MainAxisAlignment::Start);
        row->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);
        // Fixed height for list item
        // Wait, FlexLayout doesn't force height unless constrained, but we can set constraints later.
        
        txui::IconType icon_type = txui::IconType::File;
        if (item.type == FileType::Directory) icon_type = txui::IconType::Folder;
        else if (item.is_executable) icon_type = txui::IconType::Executable;
        // Archive/Image matching can be added later by mime_type

        auto icon = txui::make_ref<txui::Icon>(icon_type, 16.0);
        auto label = txui::make_ref<txui::Label>(item.name);
        label->set_font_size(14.0);
        
        // Add 8px padding before the label to separate it from the icon
        auto padded_label = txui::make_ref<txui::Padding>(txui::Insets(0, 0, 0, 8.0), label);

        row->add_child(icon);
        row->add_child(padded_label);
        
        if (item.type == FileType::Directory) {
            auto spacer = txui::FlexItem::Expanded(nullptr, 1);
            auto chevron = txui::make_ref<txui::Label>(">");
            chevron->set_font_size(14.0);
            chevron->set_color(txui::Color(150, 150, 150, 255));
            // Add right padding to the chevron so it doesn't touch the edge
            auto padded_chevron = txui::make_ref<txui::Padding>(txui::Insets(0, 8.0, 0, 0), chevron);
            
            row->add_child(spacer);
            row->add_child(padded_chevron);
        }
        
        // Add 12px horizontal and 6px vertical padding to the entire row
        auto padded_row = txui::make_ref<txui::Padding>(txui::Insets(6.0, 12.0), row);
        
        m_list_view->add_item(padded_row);
    }
    
    if (m_model->selected_index >= 0) {
        m_list_view->set_selected_index(m_model->selected_index);
    }
}

txui::Size ColumnWidget::measure_override(const txui::Constraints& constraints) noexcept {
    // Fixed width for column: 250px
    txui::Constraints list_constraints(250.0, 250.0, constraints.min_height, constraints.max_height);
    m_list_view->measure(list_constraints);
    return constraints.constrain(txui::Size(250.0, m_list_view->desired_size().height));
}

void ColumnWidget::layout_override(const txui::Rect& frame) noexcept {
    m_list_view->layout(txui::Rect(frame.left(), frame.top(), frame.width(), frame.height()));
}

void ColumnWidget::paint_override(txui::Painter& painter) const noexcept {
    m_list_view->paint(painter);
    
    // Draw column border on the right edge, unless this is the deepest column
    if (!m_is_last_column) {
        txui::Point p1(frame().right(), frame().top());
        txui::Point p2(frame().right(), frame().bottom());
        painter.draw_line(p1, p2, 1.0, txui::Color(60, 60, 60, 255));
    }
}

bool ColumnWidget::handle_event(const txui::Event& event) noexcept {
    return m_list_view->handle_event(event);
}

} // namespace tinexus::files::ui
