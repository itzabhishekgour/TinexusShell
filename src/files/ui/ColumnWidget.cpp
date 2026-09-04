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

        txui::IconType icon_type = txui::IconType::File;
        if (item.type == FileType::Directory) {
            icon_type = txui::IconType::Folder;
        } else if (item.is_executable) {
            icon_type = txui::IconType::Executable;
        } else if (item.mime_type.starts_with("image/")) {
            icon_type = txui::IconType::Image;
        } else if (item.mime_type == "application/archive") {
            icon_type = txui::IconType::Archive;
        } else if (item.name == "Trash" || item.path.string().find("Trash") != std::string::npos) {
            icon_type = txui::IconType::Trash;
        }

        auto icon = txui::make_ref<txui::Icon>(icon_type, 16.0);
        auto label = txui::make_ref<txui::Label>(item.name);
        label->set_font_size(13.0);
        if (item.is_hidden) {
            label->set_color(txui::Color(150, 150, 165, 180));
        }
        
        // 8px padding before label
        auto padded_label = txui::make_ref<txui::Padding>(txui::Insets(0, 0, 0, 8.0), label);

        row->add_child(icon);
        row->add_child(padded_label);
        
        if (item.type == FileType::Directory) {
            auto spacer = txui::FlexItem::Expanded(nullptr, 1);
            auto chevron = txui::make_ref<txui::Label>(">");
            chevron->set_font_size(12.0);
            chevron->set_color(txui::Color(140, 145, 165, 200));
            auto padded_chevron = txui::make_ref<txui::Padding>(txui::Insets(0, 8.0, 0, 0), chevron);
            
            row->add_child(spacer);
            row->add_child(padded_chevron);
        } else {
            auto spacer = txui::FlexItem::Expanded(nullptr, 1);
            std::string size_str;
            if (item.size_bytes < 1024) {
                size_str = std::to_string(item.size_bytes) + " B";
            } else if (item.size_bytes < 1024 * 1024) {
                size_str = std::to_string(item.size_bytes / 1024) + " KB";
            } else {
                size_str = std::to_string(item.size_bytes / (1024 * 1024)) + " MB";
            }
            auto size_lbl = txui::make_ref<txui::Label>(size_str);
            size_lbl->set_font_size(11.0);
            size_lbl->set_color(txui::Color(130, 135, 150, 180));
            auto padded_size = txui::make_ref<txui::Padding>(txui::Insets(0, 6.0, 0, 0), size_lbl);
            
            row->add_child(spacer);
            row->add_child(padded_size);
        }
        
        auto padded_row = txui::make_ref<txui::Padding>(txui::Insets(5.0, 10.0), row);
        m_list_view->add_item(padded_row);
    }
    
    if (m_model->selected_index >= 0) {
        m_list_view->set_selected_index(m_model->selected_index);
    }
}

txui::Size ColumnWidget::measure_override(const txui::Constraints& constraints) noexcept {
    txui::Constraints list_constraints(240.0, 240.0, constraints.min_height, constraints.max_height);
    m_list_view->measure(list_constraints);
    return constraints.constrain(txui::Size(240.0, m_list_view->desired_size().height));
}

void ColumnWidget::layout_override(const txui::Rect& frame) noexcept {
    m_list_view->layout(txui::Rect(frame.left(), frame.top(), frame.width(), frame.height()));
}

void ColumnWidget::paint_override(txui::Painter& painter) const noexcept {
    m_list_view->paint(painter);
    
    // Draw subtle vertical column separator line on the right edge
    if (!m_is_last_column) {
        painter.fill_rect(txui::Rect(frame().right() - 1.0, frame().top(), 1.0, frame().height()), txui::Color(55, 58, 72, 255));
    }
}

bool ColumnWidget::handle_event(const txui::Event& event) noexcept {
    return m_list_view->handle_event(event);
}

} // namespace tinexus::files::ui
