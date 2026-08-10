#include "ColumnWidget.hpp"
#include <txui/widgets/Icon.hpp>
#include <txui/widgets/Label.hpp>
#include <txui/layout/FlexLayout.hpp>

namespace tinexus::files::ui {

ColumnWidget::ColumnWidget(const ColumnLevel& level, size_t col_index) 
    : m_model(&level), m_col_index(col_index) {
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
        
        row->add_child(icon);
        row->add_child(label);
        
        m_list_view->add_item(row);
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
    // Draw column border
    painter.fill_rect(txui::Rect(frame().right() - 1.0, frame().top(), 1.0, frame().height()), txui::Color(30, 30, 30, 255));
    m_list_view->paint(painter);
}

bool ColumnWidget::handle_event(const txui::Event& event) noexcept {
    return m_list_view->handle_event(event);
}

} // namespace tinexus::files::ui
