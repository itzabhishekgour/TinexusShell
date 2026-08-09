#include "ColumnBrowserWidget.hpp"
#include "ColumnWidget.hpp"

namespace tinexus::files::ui {

ColumnBrowserWidget::ColumnBrowserWidget() {
    m_scroll_area = std::make_shared<txui::ScrollArea>();
    m_columns_layout = std::make_shared<txui::FlexLayout>(txui::FlexDirection::Row, txui::FlexAlignment::Start, txui::CrossAxisAlignment::Stretch);
    
    m_scroll_area->add_child(m_columns_layout);
    add_child(m_scroll_area);
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
        // Model updated (either new column added, or selection changed)
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
        mark_needs_measure();
        
        // Auto-scroll to the rightmost column
        m_scroll_area->set_scroll_x(10000.0); // Hacky way to force max scroll, layout will clamp it
    }
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

txui::Size ColumnBrowserWidget::measure_override(const txui::Constraints& constraints) noexcept {
    m_scroll_area->measure(constraints);
    return constraints.constrain(m_scroll_area->desired_size());
}

void ColumnBrowserWidget::layout_override(const txui::Rect& frame) noexcept {
    m_scroll_area->layout(txui::Rect(frame.left(), frame.top(), frame.width(), frame.height()));
}

void ColumnBrowserWidget::paint_override(txui::Painter& painter) const noexcept {
    painter.fill_rect(frame(), txui::Color(30, 30, 30, 255));
    m_scroll_area->paint(painter);
}

bool ColumnBrowserWidget::handle_event(const txui::Event& event) noexcept {
    return m_scroll_area->handle_event(event);
}

} // namespace tinexus::files::ui
