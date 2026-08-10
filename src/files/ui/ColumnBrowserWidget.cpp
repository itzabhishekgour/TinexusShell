#include "ColumnBrowserWidget.hpp"
#include "ColumnWidget.hpp"
#include <txui/widgets/Label.hpp>
#include <txui/widgets/Icon.hpp>
#include <txui/layout/Padding.hpp>

namespace tinexus::files::ui {

ColumnBrowserWidget::ColumnBrowserWidget() {
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
    
    m_path_label = txui::make_ref<txui::Label>("/");
    m_path_label->set_font_size(14.0);
    m_path_label->set_color(txui::Color(200, 200, 200, 255));

    // Sidebar setup
    m_sidebar = txui::make_ref<txui::FlexLayout>();
    m_sidebar->set_direction(txui::FlexDirection::Column);
    m_sidebar->set_main_axis_alignment(txui::MainAxisAlignment::Start);
    m_sidebar->set_cross_axis_alignment(txui::CrossAxisAlignment::Stretch);
    
    auto create_sidebar_item = [](const std::string& text, txui::IconType icon_type) {
        auto row = txui::make_ref<txui::FlexLayout>();
        row->set_direction(txui::FlexDirection::Row);
        row->set_main_axis_alignment(txui::MainAxisAlignment::Start);
        row->set_cross_axis_alignment(txui::CrossAxisAlignment::Center);

        auto icon = txui::make_ref<txui::Icon>(icon_type, 16.0);
        auto label = txui::make_ref<txui::Label>(text);
        label->set_font_size(14.0);
        label->set_color(txui::Color(200, 200, 200, 255));
        
        auto padded_label = txui::make_ref<txui::Padding>(txui::Insets(0, 0, 0, 8.0), label);
        
        row->add_child(icon);
        row->add_child(padded_label);
        
        return txui::make_ref<txui::Padding>(txui::Insets(8.0, 16.0), row);
    };
    
    m_sidebar_home = create_sidebar_item("Home", txui::IconType::Folder);
    m_sidebar_downloads = create_sidebar_item("Downloads", txui::IconType::Folder);
    m_sidebar_apps = create_sidebar_item("Apps", txui::IconType::Executable);
    m_sidebar_trash = create_sidebar_item("Trash", txui::IconType::Folder);
    
    m_sidebar->add_child(m_sidebar_home);
    m_sidebar->add_child(m_sidebar_downloads);
    m_sidebar->add_child(m_sidebar_apps);
    m_sidebar->add_child(m_sidebar_trash);

    add_child(m_sidebar);
    add_child(m_scroll_area);
    add_child(m_preview_panel);
    add_child(m_path_label);
}

void ColumnBrowserWidget::navigate_to(const std::filesystem::path& path) {
    m_model.initialize(path);
    m_columns_layout->remove_all_children();
    
    size_t col_index = 0;
    size_t total_cols = m_model.columns().size();
    for (const auto& level : m_model.columns()) {
        bool is_last = (col_index == total_cols - 1);
        auto col_widget = txui::make_ref<ColumnWidget>(level, col_index, is_last);
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
        size_t total_cols = m_model.columns().size();
        for (const auto& level : m_model.columns()) {
            bool is_last = (c == total_cols - 1);
            auto col_widget = txui::make_ref<ColumnWidget>(level, c, is_last);
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
    
    // Update path bar
    std::string breadcrumb = "";
    for (const auto& col : m_model.columns()) {
        if (!breadcrumb.empty()) breadcrumb += " > ";
        std::string name = col.directory_path.filename().string();
        if (name.empty() || name == "/") name = "System";
        breadcrumb += name;
    }
    
    auto selected_path = get_selected_path();
    if (std::filesystem::is_regular_file(selected_path)) {
        if (!breadcrumb.empty()) breadcrumb += " > ";
        breadcrumb += selected_path.filename().string();
    }
    
    m_path_label->set_text(breadcrumb);
    
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
    double path_bar_height = 24.0;
    double top_height = std::max(0.0, constraints.max_height - path_bar_height);
    double sidebar_width = 120.0;
    double preview_width = 250.0;

    txui::Constraints sidebar_c(sidebar_width, sidebar_width, 0.0, top_height);
    txui::Constraints scroll_c(0.0, std::max(0.0, constraints.max_width - sidebar_width - preview_width), 0.0, top_height);
    txui::Constraints preview_c(preview_width, preview_width, 0.0, top_height);
    
    m_sidebar->measure(sidebar_c);
    m_scroll_area->measure(scroll_c);
    m_preview_panel->measure(preview_c);
    
    txui::Constraints path_c(0.0, constraints.max_width, path_bar_height, path_bar_height);
    m_path_label->measure(path_c);
    
    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void ColumnBrowserWidget::layout_override(const txui::Rect& frame) noexcept {
    double path_bar_height = 24.0;
    double top_height = std::max(0.0, frame.height() - path_bar_height);
    double sidebar_width = 120.0;
    double preview_width = 250.0;
    double scroll_w = std::max(0.0, frame.width() - sidebar_width - preview_width);
    
    m_sidebar->layout(txui::Rect(frame.left(), frame.top(), sidebar_width, top_height));
    m_scroll_area->layout(txui::Rect(frame.left() + sidebar_width, frame.top(), scroll_w, top_height));
    m_preview_panel->layout(txui::Rect(frame.left() + sidebar_width + scroll_w, frame.top(), preview_width, top_height));
    m_path_label->layout(txui::Rect(frame.left() + 10.0, frame.top() + top_height, std::max(0.0, frame.width() - 10.0), path_bar_height));
}

void ColumnBrowserWidget::paint_override(txui::Painter& painter) const noexcept {
    painter.fill_rect(frame(), txui::Color(30, 30, 30, 255));
    
    // Draw Sidebar Background
    painter.fill_rect(m_sidebar->frame(), txui::Color(35, 35, 35, 255));
    m_sidebar->paint(painter);
    
    m_scroll_area->paint(painter);
    
    // Draw Preview Panel Background
    painter.fill_rect(m_preview_panel->frame(), txui::Color(40, 40, 40, 255));
    m_preview_panel->paint(painter);
    
    // Draw Path Bar Background
    double path_bar_height = 24.0;
    double top_height = std::max(0.0, frame().height() - path_bar_height);
    painter.fill_rect(txui::Rect(frame().left(), frame().top() + top_height, frame().width(), path_bar_height), txui::Color(25, 25, 25, 255));
    
    m_path_label->paint(painter);
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
    
    if (event.type == txui::EventType::PointerButtonPress) {
        auto check_sidebar_click = [&](txui::Ref<txui::Widget> item, const std::string& target) {
            if (item->frame().contains(event.pointer.x, event.pointer.y)) {
                navigate_to(target);
                return true;
            }
            return false;
        };
        
        if (check_sidebar_click(m_sidebar_home, "/home/tinexus")) return true;
        if (check_sidebar_click(m_sidebar_downloads, "/home/tinexus/Downloads")) return true;
        if (check_sidebar_click(m_sidebar_apps, "/opt/tinexus-apps")) return true;
        if (check_sidebar_click(m_sidebar_trash, "/home/tinexus/.local/share/Trash/files")) return true;
    }
    
    // Since we removed m_root_layout, we should forward events to our children
    if (m_scroll_area->handle_event(event)) return true;
    if (m_preview_panel->handle_event(event)) return true;
    
    return false;
}

} // namespace tinexus::files::ui
