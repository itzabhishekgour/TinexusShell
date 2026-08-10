#include "ColumnBrowserWidget.hpp"
#include "ColumnWidget.hpp"
#include <txui/widgets/Label.hpp>

namespace tinexus::files::ui {

ColumnBrowserWidget::ColumnBrowserWidget() {
    m_root_layout = txui::make_ref<txui::FlexLayout>();
    m_root_layout->set_direction(txui::FlexDirection::Row);
    m_root_layout->set_main_axis_alignment(txui::MainAxisAlignment::Start);
    m_root_layout->set_cross_axis_alignment(txui::CrossAxisAlignment::Stretch);

    m_scroll_area = txui::make_ref<txui::ScrollArea>();
    m_columns_layout = txui::make_ref<txui::FlexLayout>();
    m_columns_layout->set_direction(txui::FlexDirection::Row);
    m_columns_layout->set_main_axis_alignment(txui::MainAxisAlignment::Start);
    m_columns_layout->set_cross_axis_alignment(txui::CrossAxisAlignment::Stretch);
    m_scroll_area->add_child(m_columns_layout);

    m_preview_panel = txui::make_ref<txui::FlexLayout>();
    m_preview_panel->set_direction(txui::FlexDirection::Column);
    m_preview_panel->set_main_axis_alignment(txui::MainAxisAlignment::Center);
    m_preview_panel->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);
    
    m_preview_name = txui::make_ref<txui::Label>("No file selected");
    m_preview_name->set_font_size(18.0);
    
    m_preview_type = txui::make_ref<txui::Label>("");
    m_preview_type->set_font_size(14.0);
    m_preview_type->set_color(txui::Color(150, 150, 150, 255));
    
    m_preview_size = txui::make_ref<txui::Label>("");
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
        auto col_widget = txui::make_ref<ColumnWidget>(level, col_index);
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
            auto col_widget = txui::make_ref<ColumnWidget>(level, c);
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
        return last_col.items[static_cast<size_t>(last_col.selected_index)].path;
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
    if (event.type == txui::EventType::KeyDown) {
        if (event.keyboard.key == txui::Key::Backspace) {
            auto path = get_selected_path();
            if (!path.empty()) {
                if (path.string().starts_with("/opt/tinexus-apps/")) {
                    if (m_on_uninstall) {
                        m_on_uninstall(path);
                        return true;
                    }
                } else {
                    if (m_on_trash) {
                        m_on_trash(path);
                        return true;
                    }
                }
            }
        }
    }
    return m_root_layout->handle_event(event);
}

} // namespace tinexus::files::ui
