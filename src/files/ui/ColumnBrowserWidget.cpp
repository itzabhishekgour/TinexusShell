#include "ColumnBrowserWidget.hpp"
#include "ColumnWidget.hpp"
#include <txui/widgets/Label.hpp>

namespace tinexus::files::ui {

ColumnBrowserWidget::ColumnBrowserWidget() {
    m_root_layout = std::make_shared<txui::FlexLayout>(txui::FlexDirection::Row, txui::FlexAlignment::Start, txui::CrossAxisAlignment::Stretch);

    m_scroll_area = std::make_shared<txui::ScrollArea>();
    m_columns_layout = std::make_shared<txui::FlexLayout>(txui::FlexDirection::Row, txui::FlexAlignment::Start, txui::CrossAxisAlignment::Stretch);
    m_scroll_area->add_child(m_columns_layout);

    m_preview_panel = std::make_shared<txui::FlexLayout>(txui::FlexDirection::Column, txui::FlexAlignment::Center, txui::CrossAxisAlignment::Center);
    
    m_preview_name = std::make_shared<txui::Label>("No file selected");
    m_preview_name->set_font_size(18.0);
    
    m_preview_type = std::make_shared<txui::Label>("");
    m_preview_type->set_font_size(14.0);
    m_preview_type->set_color(txui::Color(150, 150, 150, 255));
    
    m_preview_size = std::make_shared<txui::Label>("");
    m_preview_size->set_font_size(14.0);
    m_preview_size->set_color(txui::Color(150, 150, 150, 255));

    m_preview_panel->add_child(m_preview_name);
    m_preview_panel->add_child(m_preview_type);
    m_preview_panel->add_child(m_preview_size);

    m_root_layout->add_child(m_scroll_area);
    m_root_layout->add_child(m_preview_panel);
    
    add_child(m_root_layout);
}

void ColumnBrowserWidget::navigate_to(const std::filesystem::path& path) {
    m_model.initialize(path);
    m_columns_layout->remove_all_children();
    
    size_t col_index = 0;
    for (const auto& level : m_model.columns()) {
        auto col_widget = std::make_shared<ColumnWidget>(level, col_index);
        col_widget->set_on_item_selected([this](size_t c, size_t i) { on_item_selected(c, i); });
        col_widget->set_on_item_double_clicked([this](size_t c, size_t i) { on_item_double_clicked(c, i); });
        m_columns_layout->add_child(col_widget);
        col_index++;
    }
    mark_needs_measure();
}

void ColumnBrowserWidget::on_item_selected(size_t col_index, size_t item_index) {
    if (m_model.select_item(col_index, item_index)) {
        // Rebuild columns
        m_columns_layout->remove_all_children();
        size_t c = 0;
        for (const auto& level : m_model.columns()) {
            auto col_widget = std::make_shared<ColumnWidget>(level, c);
            col_widget->set_on_item_selected([this](size_t c_idx, size_t i_idx) { on_item_selected(c_idx, i_idx); });
            col_widget->set_on_item_double_clicked([this](size_t c_idx, size_t i_idx) { on_item_double_clicked(c_idx, i_idx); });
            m_columns_layout->add_child(col_widget);
            c++;
        }
        
        // Auto-scroll to the rightmost column
        m_scroll_area->set_scroll_x(10000.0); 
    }

    // Update preview panel
    if (col_index < m_model.columns().size()) {
        const auto& level = m_model.columns()[col_index];
        if (item_index < level.items.size()) {
            const auto& item = level.items[item_index];
            m_preview_name->set_text(item.name);
            m_preview_type->set_text(item.mime_type);
            m_preview_size->set_text(std::to_string(item.size_bytes / 1024) + " KB");
        }
    }
    
    mark_needs_measure();
}

void ColumnBrowserWidget::on_item_double_clicked(size_t col_index, size_t item_index) {
    if (col_index < m_model.columns().size()) {
        const auto& level = m_model.columns()[col_index];
        if (item_index < level.items.size()) {
            const auto& item = level.items[item_index];
            if (item.type != FileType::Directory && m_on_execute) {
                m_on_execute(item.path);
            }
        }
    }
}

std::filesystem::path ColumnBrowserWidget::get_selected_path() const {
    if (m_model.columns().empty()) return "";
    const auto& last_col = m_model.columns().back();
    if (last_col.selected_index >= 0 && last_col.selected_index < (int)last_col.items.size()) {
        return last_col.items[last_col.selected_index].path;
    }
    return last_col.directory_path;
}

txui::Size ColumnBrowserWidget::measure_override(const txui::Constraints& constraints) noexcept {
    // We hardcode preview panel to 250px width, and scroll area gets the rest
    txui::Constraints scroll_c(0.0, std::max(0.0, constraints.max_width - 250.0), 0.0, constraints.max_height);
    txui::Constraints preview_c(250.0, 250.0, 0.0, constraints.max_height);
    
    m_scroll_area->measure(scroll_c);
    m_preview_panel->measure(preview_c);
    
    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void ColumnBrowserWidget::layout_override(const txui::Rect& frame) noexcept {
    double scroll_w = std::max(0.0, frame.width() - 250.0);
    m_scroll_area->layout(txui::Rect(frame.left(), frame.top(), scroll_w, frame.height()));
    m_preview_panel->layout(txui::Rect(frame.left() + scroll_w, frame.top(), 250.0, frame.height()));
}

void ColumnBrowserWidget::paint_override(txui::Painter& painter) const noexcept {
    painter.fill_rect(frame(), txui::Color(30, 30, 30, 255));
    m_scroll_area->paint(painter);
    
    // Draw Preview Panel Background
    painter.fill_rect(m_preview_panel->frame(), txui::Color(40, 40, 40, 255));
    m_preview_panel->paint(painter);
}

bool ColumnBrowserWidget::handle_event(const txui::Event& event) noexcept {
    if (const auto* key = std::get_if<txui::KeyboardEvent>(&event)) {
        if (key->state == txui::ButtonState::Pressed && key->key == txui::Key::Delete) {
            auto path = get_selected_path();
            if (!path.empty() && m_on_trash) {
                m_on_trash(path);
                return true;
            }
        }
    }
    return m_root_layout->handle_event(event);
}

} // namespace tinexus::files::ui
