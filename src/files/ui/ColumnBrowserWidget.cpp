#include "ColumnBrowserWidget.hpp"
#include "ColumnWidget.hpp"
#include <txui/widgets/Label.hpp>
#include <txui/widgets/Icon.hpp>
#include <txui/layout/Padding.hpp>
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace tinexus::files::ui {

static std::string format_file_size(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    if (bytes < 1024 * 1024 * 1024) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        return std::string(buf);
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    return std::string(buf);
}

static std::string get_home_dir() {
    const char* home = std::getenv("HOME");
    if (home && *home) return std::string(home);
    return "/home/tinexus";
}

ColumnBrowserWidget::ColumnBrowserWidget() {
    m_scroll_area = txui::make_ref<txui::ScrollArea>();
    m_scroll_area->set_allow_scroll_y(false);
    
    m_columns_layout = txui::make_ref<txui::FlexLayout>();
    m_columns_layout->set_direction(txui::FlexDirection::Row);
    m_columns_layout->set_main_axis_alignment(txui::MainAxisAlignment::Start);
    m_columns_layout->set_cross_axis_alignment(txui::CrossAxisAlignment::Stretch);
    m_scroll_area->add_child(m_columns_layout);

    std::string home = get_home_dir();
    m_bookmarks = {
        {"Home",        home,                           txui::IconType::Home},
        {"Desktop",     home + "/Desktop",              txui::IconType::Apps},
        {"Documents",   home + "/Documents",            txui::IconType::File},
        {"Downloads",   home + "/Downloads",            txui::IconType::Downloads},
        {"Apps",        "/opt/tinexus-apps",            txui::IconType::Apps},
        {"File System", "/",                            txui::IconType::Folder},
        {"Trash",       home + "/.local/share/Trash/files", txui::IconType::Trash}
    };

    add_child(m_scroll_area);
}

void ColumnBrowserWidget::rebuild_breadcrumbs() {
    m_breadcrumbs.clear();
    std::filesystem::path cur = m_current_root;
    std::vector<std::filesystem::path> parts;
    
    while (!cur.empty() && cur != cur.root_path()) {
        parts.push_back(cur);
        cur = cur.parent_path();
    }
    if (m_current_root == m_current_root.root_path() || parts.empty()) {
        parts.push_back(m_current_root.root_path());
    } else {
        parts.push_back(m_current_root.root_path());
    }
    std::reverse(parts.begin(), parts.end());

    for (const auto& p : parts) {
        std::string name = p.filename().string();
        if (name.empty() || p == p.root_path()) {
            name = "Root";
        }
        m_breadcrumbs.push_back({name, p, txui::Rect{}, false});
    }
}

void ColumnBrowserWidget::rebuild_columns() {
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
    
    m_scroll_area->set_scroll_x(10000.0);
    mark_needs_measure();
}

void ColumnBrowserWidget::navigate_to(const std::filesystem::path& path, bool record_history) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        std::filesystem::create_directories(path, ec);
    }

    m_current_root = std::filesystem::canonical(path, ec);
    if (ec) m_current_root = path;

    if (record_history) {
        if (m_history.empty() || m_history[m_history_idx] != m_current_root) {
            if (m_history_idx + 1 < m_history.size()) {
                m_history.resize(m_history_idx + 1);
            }
            m_history.push_back(m_current_root);
            m_history_idx = m_history.size() - 1;
        }
    }

    m_model.initialize(m_current_root);
    rebuild_breadcrumbs();
    rebuild_columns();
    update_inspector_state(m_model.columns().size() - 1, static_cast<size_t>(-1));
}

void ColumnBrowserWidget::go_back() {
    if (m_history_idx > 0) {
        m_history_idx--;
        navigate_to(m_history[m_history_idx], false);
    }
}

void ColumnBrowserWidget::go_forward() {
    if (m_history_idx + 1 < m_history.size()) {
        m_history_idx++;
        navigate_to(m_history[m_history_idx], false);
    }
}

void ColumnBrowserWidget::go_up() {
    if (m_current_root != m_current_root.root_path()) {
        navigate_to(m_current_root.parent_path(), true);
    }
}

void ColumnBrowserWidget::on_item_selected(size_t col_index, size_t item_index) {
    if (m_model.select_item(col_index, item_index)) {
        rebuild_columns();
    }
    update_inspector_state(col_index, item_index);
}

void ColumnBrowserWidget::on_item_double_clicked(size_t col_index, size_t item_index) {
    if (col_index < m_model.columns().size()) {
        const auto& level = m_model.columns()[col_index];
        if (item_index < level.items.size()) {
            const auto& item = level.items[item_index];
            if (item.type == FileType::Directory) {
                navigate_to(item.path);
            } else if (m_on_execute) {
                m_on_execute(item.path);
            }
        }
    }
}

void ColumnBrowserWidget::update_inspector_state(size_t col_index, size_t item_index) {
    if (col_index < m_model.columns().size()) {
        const auto& level = m_model.columns()[col_index];
        if (item_index < level.items.size()) {
            const auto& item = level.items[item_index];
            m_has_selection = true;
            m_inspector_name = item.name;
            m_inspector_path = item.path.string();
            
            if (item.type == FileType::Directory) {
                m_inspector_type = "Folder";
                m_inspector_icon = txui::IconType::Folder;
                m_inspector_size = std::to_string(FileModel::scan_directory(item.path).size()) + " items";
            } else if (item.is_executable) {
                m_inspector_type = "Application / Binary";
                m_inspector_icon = txui::IconType::Executable;
                m_inspector_size = format_file_size(item.size_bytes);
            } else if (item.mime_type.starts_with("image/")) {
                m_inspector_type = "Image (" + item.mime_type + ")";
                m_inspector_icon = txui::IconType::Image;
                m_inspector_size = format_file_size(item.size_bytes);
            } else if (item.mime_type == "application/archive") {
                m_inspector_type = "Archive File";
                m_inspector_icon = txui::IconType::Archive;
                m_inspector_size = format_file_size(item.size_bytes);
            } else {
                m_inspector_type = item.mime_type;
                m_inspector_icon = txui::IconType::File;
                m_inspector_size = format_file_size(item.size_bytes);
            }
            m_inspector_perms = "Readable / Writable";
            mark_needs_paint();
            return;
        }
    }

    // Default inspector when no item is selected
    m_has_selection = false;
    m_inspector_name = m_current_root.filename().string();
    if (m_inspector_name.empty()) m_inspector_name = "Root";
    m_inspector_type = "Directory";
    m_inspector_path = m_current_root.string();
    m_inspector_size = std::to_string(FileModel::scan_directory(m_current_root).size()) + " items";
    m_inspector_perms = "Directory";
    m_inspector_icon = txui::IconType::Folder;
    mark_needs_paint();
}

std::filesystem::path ColumnBrowserWidget::get_selected_path() const {
    if (m_model.columns().empty()) return m_current_root;
    const auto& last_col = m_model.columns().back();
    if (last_col.selected_index >= 0 && last_col.selected_index < static_cast<int>(last_col.items.size())) {
        return last_col.items[static_cast<size_t>(last_col.selected_index)].path;
    }
    return last_col.directory_path;
}

txui::Size ColumnBrowserWidget::measure_override(const txui::Constraints& constraints) noexcept {
    double toolbar_h = 44.0;
    double status_h  = 26.0;
    double middle_h  = std::max(0.0, constraints.max_height - toolbar_h - status_h);
    double sidebar_w = 170.0;
    double inspector_w = 230.0;
    double scroll_w  = std::max(0.0, constraints.max_width - sidebar_w - inspector_w);

    txui::Constraints scroll_c(scroll_w, scroll_w, middle_h, middle_h);
    m_scroll_area->measure(scroll_c);

    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void ColumnBrowserWidget::layout_override(const txui::Rect& frame) noexcept {
    double toolbar_h = 44.0;
    double status_h  = 26.0;
    double middle_h  = std::max(0.0, frame.height() - toolbar_h - status_h);
    double sidebar_w = 170.0;
    double inspector_w = 230.0;
    double scroll_w  = std::max(0.0, frame.width() - sidebar_w - inspector_w);

    // Toolbar buttons layout
    double btn_y = frame.top() + 8.0;
    double btn_size = 28.0;
    m_back_btn_rect = txui::Rect(frame.left() + 12.0, btn_y, btn_size, btn_size);
    m_fwd_btn_rect  = txui::Rect(frame.left() + 12.0 + btn_size + 6.0, btn_y, btn_size, btn_size);
    m_up_btn_rect   = txui::Rect(frame.left() + 12.0 + (btn_size + 6.0) * 2.0, btn_y, btn_size, btn_size);

    // Breadcrumbs layout
    double cur_bc_x = frame.left() + 12.0 + (btn_size + 6.0) * 3.0 + 12.0;
    for (auto& bc : m_breadcrumbs) {
        double pill_w = static_cast<double>(bc.name.size()) * 7.5 + 20.0;
        bc.rect = txui::Rect(cur_bc_x, btn_y + 2.0, pill_w, 24.0);
        cur_bc_x += pill_w + 14.0; // 14px space for "/" separator
    }

    // Sidebar items layout
    double sb_y = frame.top() + toolbar_h + 30.0; // below "FAVORITES" label
    for (auto& bm : m_bookmarks) {
        bm.rect = txui::Rect(frame.left() + 8.0, sb_y, sidebar_w - 16.0, 32.0);
        sb_y += 36.0;
    }

    // Center scroll area
    m_scroll_area->layout(txui::Rect(frame.left() + sidebar_w, frame.top() + toolbar_h, scroll_w, middle_h));

    // Inspector action buttons
    double insp_x = frame.left() + sidebar_w + scroll_w;
    double insp_y = frame.top() + toolbar_h;
    m_open_btn_rect  = txui::Rect(insp_x + 20.0, insp_y + middle_h - 75.0, inspector_w - 40.0, 30.0);
    m_trash_btn_rect = txui::Rect(insp_x + 20.0, insp_y + middle_h - 38.0, inspector_w - 40.0, 26.0);
}

void ColumnBrowserWidget::paint_override(txui::Painter& painter) const noexcept {
    const double gx = frame().left();
    const double gy = frame().top();
    const double W  = frame().width();
    const double H  = frame().height();
    const double toolbar_h = 44.0;
    const double status_h  = 26.0;
    const double middle_h  = std::max(0.0, H - toolbar_h - status_h);
    const double sidebar_w = 170.0;
    const double inspector_w = 230.0;
    const double scroll_w  = std::max(0.0, W - sidebar_w - inspector_w);

    // ── Main Background ──────────────────────────────────────────────────────────
    painter.fill_rect(frame(), txui::Color(26, 28, 38, 255));

    // ── 1. Top Navigation Toolbar ────────────────────────────────────────────────
    painter.fill_gradient_rect(
        txui::Rect(gx, gy, W, toolbar_h),
        txui::Color(34, 36, 50, 255),
        txui::Color(28, 30, 42, 255)
    );
    painter.fill_rect(txui::Rect(gx, gy + toolbar_h - 1.0, W, 1.0), txui::Color(48, 52, 70, 255));

    // Nav Buttons (<, >, ^)
    auto draw_nav_btn = [&](const txui::Rect& r, const std::string& symbol, bool hovered, bool enabled) {
        txui::Color bg = hovered && enabled ? txui::Color(65, 70, 95, 255) : txui::Color(42, 45, 62, 255);
        txui::Color fg = enabled ? txui::Color(230, 235, 250, 255) : txui::Color(100, 105, 125, 180);
        painter.fill_rounded_rect(r, 6.0, bg);
        painter.draw_text(txui::Point(r.left() + (r.width() - 8.0)/2.0, r.top() + (r.height() - 14.0)/2.0), symbol, fg, 13.0, true);
    };

    draw_nav_btn(m_back_btn_rect, "<", m_back_hovered, m_history_idx > 0);
    draw_nav_btn(m_fwd_btn_rect,  ">", m_fwd_hovered,  m_history_idx + 1 < m_history.size());
    draw_nav_btn(m_up_btn_rect,   "^", m_up_hovered,   m_current_root != m_current_root.root_path());

    // Breadcrumbs Path
    for (size_t i = 0; i < m_breadcrumbs.size(); ++i) {
        const auto& bc = m_breadcrumbs[i];
        bool is_last = (i == m_breadcrumbs.size() - 1);
        txui::Color pill_bg = bc.hovered ? txui::Color(60, 65, 90, 255) : (is_last ? txui::Color(45, 50, 72, 200) : txui::Color(35, 38, 54, 160));
        txui::Color text_fg = is_last ? txui::Color(255, 255, 255, 255) : txui::Color(190, 195, 215, 230);

        painter.fill_rounded_rect(bc.rect, 5.0, pill_bg);
        painter.draw_text(txui::Point(bc.rect.left() + 8.0, bc.rect.top() + 5.0), bc.name, text_fg, 12.0, is_last);

        if (!is_last) {
            painter.draw_text(txui::Point(bc.rect.right() + 4.0, bc.rect.top() + 5.0), "/", txui::Color(110, 115, 135, 200), 12.0);
        }
    }

    // ── 2. Left Places Sidebar ───────────────────────────────────────────────────
    txui::Rect sidebar_rect(gx, gy + toolbar_h, sidebar_w, middle_h);
    painter.fill_rect(sidebar_rect, txui::Color(22, 24, 34, 255));
    painter.fill_rect(txui::Rect(sidebar_rect.right() - 1.0, sidebar_rect.top(), 1.0, sidebar_rect.height()), txui::Color(45, 48, 64, 255));

    // Section header
    painter.draw_text(txui::Point(gx + 16.0, gy + toolbar_h + 12.0), "FAVORITES", txui::Color(115, 122, 145, 255), 11.0, true);

    // Bookmarks
    for (const auto& bm : m_bookmarks) {
        bool is_active = (m_current_root == bm.path);
        if (is_active) {
            painter.fill_rounded_rect(bm.rect, 7.0, txui::Color(45, 110, 225, 220));
            // Left accent line
            painter.fill_rounded_rect(txui::Rect(bm.rect.left() + 2.0, bm.rect.top() + 5.0, 3.0, bm.rect.height() - 10.0), 1.5, txui::Color(255, 255, 255, 255));
        } else if (bm.hovered) {
            painter.fill_rounded_rect(bm.rect, 7.0, txui::Color(255, 255, 255, 16));
        }

        // Bookmark Icon symbol & Text
        txui::Color icon_col = is_active ? txui::Color(255, 255, 255, 255) : txui::Color(130, 165, 245, 240);
        double icon_cx = bm.rect.left() + 18.0;
        double icon_cy = bm.rect.top() + bm.rect.height() / 2.0;

        // Custom icon miniature
        if (bm.icon_type == txui::IconType::Home) {
            painter.fill_circle(txui::Point(icon_cx, icon_cy), 5.5, icon_col);
        } else if (bm.icon_type == txui::IconType::Trash) {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 5.0, icon_cy - 5.0, 10.0, 10.0), 2.0, txui::Color(240, 80, 80, 240));
        } else {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 5.0, icon_cy - 5.0, 10.0, 10.0), 2.0, icon_col);
        }

        txui::Color text_col = is_active ? txui::Color(255, 255, 255, 255) : txui::Color(210, 215, 230, 240);
        painter.draw_text(txui::Point(bm.rect.left() + 32.0, bm.rect.top() + 8.5), bm.name, text_col, 13.0, is_active);
    }

    // ── 3. Center Miller Columns ─────────────────────────────────────────────────
    m_scroll_area->paint(painter);

    // ── 4. Right Inspector / Preview Panel ───────────────────────────────────────
    txui::Rect insp_rect(gx + sidebar_w + scroll_w, gy + toolbar_h, inspector_w, middle_h);
    painter.fill_rect(insp_rect, txui::Color(20, 22, 32, 255));
    painter.fill_rect(txui::Rect(insp_rect.left(), insp_rect.top(), 1.0, insp_rect.height()), txui::Color(45, 48, 64, 255));

    // Big Icon
    double big_icon_cx = insp_rect.left() + insp_rect.width() / 2.0;
    double big_icon_cy = insp_rect.top() + 45.0;
    txui::Color big_icon_bg = m_has_selection ? txui::Color(40, 120, 240, 255) : txui::Color(60, 65, 85, 255);
    painter.fill_rounded_rect(txui::Rect(big_icon_cx - 24.0, big_icon_cy - 24.0, 48.0, 48.0), 12.0, big_icon_bg);
    painter.fill_circle(txui::Point(big_icon_cx, big_icon_cy), 8.0, txui::Color(255, 255, 255, 220));

    // File name
    std::string display_name = m_inspector_name;
    if (display_name.size() > 22) display_name = display_name.substr(0, 19) + "...";
    painter.draw_text(txui::Point(insp_rect.left() + 16.0, insp_rect.top() + 85.0), display_name, txui::Color(255, 255, 255, 255), 14.0, true);

    // Kind / MIME
    painter.draw_text(txui::Point(insp_rect.left() + 16.0, insp_rect.top() + 110.0), "KIND", txui::Color(120, 125, 145, 200), 10.5, true);
    painter.draw_text(txui::Point(insp_rect.left() + 16.0, insp_rect.top() + 125.0), m_inspector_type, txui::Color(210, 215, 230, 240), 12.0);

    // Size
    painter.draw_text(txui::Point(insp_rect.left() + 16.0, insp_rect.top() + 150.0), "SIZE", txui::Color(120, 125, 145, 200), 10.5, true);
    painter.draw_text(txui::Point(insp_rect.left() + 16.0, insp_rect.top() + 165.0), m_inspector_size, txui::Color(210, 215, 230, 240), 12.0);

    // Permissions
    painter.draw_text(txui::Point(insp_rect.left() + 16.0, insp_rect.top() + 190.0), "PERMISSIONS", txui::Color(120, 125, 145, 200), 10.5, true);
    painter.draw_text(txui::Point(insp_rect.left() + 16.0, insp_rect.top() + 205.0), m_inspector_perms, txui::Color(210, 215, 230, 240), 12.0);

    // Action Buttons
    if (m_has_selection) {
        // Open / Execute button
        txui::Color open_bg = m_open_hovered ? txui::Color(60, 135, 255, 255) : txui::Color(40, 110, 235, 255);
        painter.fill_rounded_rect(m_open_btn_rect, 6.0, open_bg);
        painter.draw_text(txui::Point(m_open_btn_rect.left() + (m_open_btn_rect.width() - 40.0)/2.0, m_open_btn_rect.top() + 7.5), "Open", txui::Color(255, 255, 255, 255), 13.0, true);

        // Move to trash button
        txui::Color trash_bg = m_trash_hovered ? txui::Color(80, 35, 45, 255) : txui::Color(45, 25, 32, 255);
        painter.fill_rounded_rect(m_trash_btn_rect, 6.0, trash_bg);
        painter.draw_text(txui::Point(m_trash_btn_rect.left() + (m_trash_btn_rect.width() - 85.0)/2.0, m_trash_btn_rect.top() + 6.0), "Move to Trash", txui::Color(245, 120, 120, 230), 11.5);
    }

    // ── 5. Bottom Status Bar ─────────────────────────────────────────────────────
    txui::Rect status_rect(gx, gy + H - status_h, W, status_h);
    painter.fill_rect(status_rect, txui::Color(20, 21, 30, 255));
    painter.fill_rect(txui::Rect(gx, status_rect.top(), W, 1.0), txui::Color(40, 44, 58, 255));

    std::string status_msg = m_inspector_path;
    if (status_msg.size() > 70) status_msg = "..." + status_msg.substr(status_msg.size() - 67);
    painter.draw_text(txui::Point(gx + 12.0, status_rect.top() + 6.5), status_msg, txui::Color(145, 150, 170, 220), 11.5);

    std::string item_summary = m_inspector_size;
    painter.draw_text(txui::Point(gx + W - 120.0, status_rect.top() + 6.5), item_summary, txui::Color(145, 150, 170, 220), 11.5);
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

    if (event.type == txui::EventType::PointerMove) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        m_back_hovered = m_back_btn_rect.contains(px, py);
        m_fwd_hovered  = m_fwd_btn_rect.contains(px, py);
        m_up_hovered   = m_up_btn_rect.contains(px, py);

        for (auto& bc : m_breadcrumbs) {
            bc.hovered = bc.rect.contains(px, py);
        }

        for (auto& bm : m_bookmarks) {
            bm.hovered = bm.rect.contains(px, py);
        }

        m_open_hovered  = m_has_selection && m_open_btn_rect.contains(px, py);
        m_trash_hovered = m_has_selection && m_trash_btn_rect.contains(px, py);

        mark_needs_paint();
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        // Check Toolbar Nav Buttons
        if (m_back_btn_rect.contains(px, py)) {
            go_back();
            return true;
        }
        if (m_fwd_btn_rect.contains(px, py)) {
            go_forward();
            return true;
        }
        if (m_up_btn_rect.contains(px, py)) {
            go_up();
            return true;
        }

        // Check Breadcrumbs
        for (const auto& bc : m_breadcrumbs) {
            if (bc.rect.contains(px, py)) {
                navigate_to(bc.path);
                return true;
            }
        }

        // Check Sidebar Bookmarks
        for (const auto& bm : m_bookmarks) {
            if (bm.rect.contains(px, py)) {
                navigate_to(bm.path);
                return true;
            }
        }

        // Check Inspector Action Buttons
        if (m_has_selection && m_open_btn_rect.contains(px, py)) {
            auto path = get_selected_path();
            if (!path.empty()) {
                if (std::filesystem::is_directory(path)) {
                    navigate_to(path);
                } else if (m_on_execute) {
                    m_on_execute(path);
                }
            }
            return true;
        }

        if (m_has_selection && m_trash_btn_rect.contains(px, py)) {
            auto path = get_selected_path();
            if (!path.empty() && m_on_trash) {
                m_on_trash(path);
            }
            return true;
        }
    }

    if (m_scroll_area->handle_event(event)) {
        return true;
    }

    return false;
}

} // namespace tinexus::files::ui
