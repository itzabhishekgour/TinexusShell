#include "ColumnBrowserWidget.hpp"
#include "ColumnWidget.hpp"
#include "files/TagManager.hpp"
#include "files/ThumbnailCache.hpp"
#include "files/RecentManager.hpp"
#include "files/file_operations.hpp"
#include "files/FileClipboard.hpp"
#include <txui/widgets/Label.hpp>
#include <txui/widgets/Icon.hpp>
#include <txui/layout/Padding.hpp>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
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

static std::string format_file_time(std::filesystem::file_time_type ftime) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);
    std::tm tm_buf{};
    localtime_r(&cftime, &tm_buf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%b %d, %H:%M", &tm_buf);
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

    rebuild_sidebar();
    add_child(m_scroll_area);
}

void ColumnBrowserWidget::rebuild_sidebar() {
    m_sidebar_items.clear();
    std::string home = get_home_dir();

    // Section 1: FAVORITES
    m_sidebar_items.push_back({"FAVORITES", "", txui::IconType::File, txui::Rect{}, false, true});
    m_sidebar_items.push_back({"Home",        home,                           txui::IconType::Home, txui::Rect{}, false, false});
    m_sidebar_items.push_back({"Desktop",     home + "/Desktop",              txui::IconType::Apps, txui::Rect{}, false, false});
    m_sidebar_items.push_back({"Documents",   home + "/Documents",            txui::IconType::File, txui::Rect{}, false, false});
    m_sidebar_items.push_back({"Downloads",   home + "/Downloads",            txui::IconType::Downloads, txui::Rect{}, false, false});
    m_sidebar_items.push_back({"Apps",        "/opt/tinexus-apps",            txui::IconType::Apps, txui::Rect{}, false, false});

    // Section 2: LOCATIONS
    m_sidebar_items.push_back({"LOCATIONS", "", txui::IconType::File, txui::Rect{}, false, true});
    m_sidebar_items.push_back({"File System", "/", txui::IconType::Folder, txui::Rect{}, false, false});

    // Check for mounted drives in /mnt or /media
    for (const auto& loc : {"/mnt", "/media"}) {
        std::error_code ec;
        if (std::filesystem::exists(loc, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(loc, ec)) {
                if (entry.is_directory(ec)) {
                    m_sidebar_items.push_back({entry.path().filename().string(), entry.path(), txui::IconType::Folder, txui::Rect{}, false, false});
                }
            }
        }
    }

    // Section 3: RECENTS
    m_sidebar_items.push_back({"RECENTS", "", txui::IconType::File, txui::Rect{}, false, true});
    m_sidebar_items.push_back({"Recent (Session)", home + "/.recents_view", txui::IconType::File, txui::Rect{}, false, false});

    // Trash
    m_sidebar_items.push_back({"Trash", home + "/.local/share/Trash/files", txui::IconType::Trash, txui::Rect{}, false, false});
}

void ColumnBrowserWidget::rebuild_breadcrumbs() {
    m_breadcrumbs.clear();
    std::filesystem::path cur = m_current_root;
    if (m_view_mode == ViewMode::Column) {
        if (!m_selected_path.empty()) {
            cur = m_selected_path;
        } else if (!m_model.columns().empty()) {
            cur = m_model.columns().back().directory_path;
        }
    }

    std::vector<std::filesystem::path> parts;
    while (!cur.empty() && cur != cur.root_path()) {
        parts.push_back(cur);
        cur = cur.parent_path();
    }
    parts.push_back(m_current_root.root_path());
    std::reverse(parts.begin(), parts.end());

    for (const auto& p : parts) {
        std::string name = p.filename().string();
        if (name.empty() || p == p.root_path()) {
            name = "Root";
        }
        m_breadcrumbs.push_back({name, p, txui::Rect{}, false});
    }
}

void ColumnBrowserWidget::refresh_current_directory() {
    m_current_items = FileModel::scan_directory(m_current_root, false);
    FileModel::sort_items(m_current_items, m_sort_criteria, m_sort_direction);
    m_selected_item_idx = -1;
    if (!m_current_items.empty()) {
        m_selected_item_idx = 0;
        m_selected_path = m_current_items[0].path;
        update_inspector_from_path(m_selected_path);
    } else {
        m_selected_path.clear();
        update_inspector_from_path(m_current_root);
    }
}

std::vector<FileItem> ColumnBrowserWidget::get_display_items() const {
    if (m_search_query.empty()) {
        return m_current_items;
    }

    std::string query_lower = m_search_query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), [](unsigned char c) { return std::tolower(c); });

    std::vector<FileItem> filtered;
    for (const auto& item : m_current_items) {
        std::string name_lower = item.name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), [](unsigned char c) { return std::tolower(c); });
        if (name_lower.find(query_lower) != std::string::npos) {
            filtered.push_back(item);
        }
    }
    return filtered;
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

    double total_w = static_cast<double>(total_cols) * 240.0;
    double visible_w = std::max(100.0, frame().width() - 170.0 - 230.0);
    double target_scroll = std::max(0.0, total_w - visible_w);
    m_scroll_area->set_scroll_x(target_scroll);
    mark_needs_measure();
    mark_needs_layout();
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

    m_scroll_y = 0.0;
    refresh_current_directory();
    m_model.initialize(m_current_root);
    rebuild_breadcrumbs();
    rebuild_columns();
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

void ColumnBrowserWidget::set_view_mode(ViewMode mode) {
    m_view_mode = mode;
    m_scroll_y = 0.0;
    if (mode == ViewMode::Column) {
        rebuild_columns();
        rebuild_breadcrumbs();
    }
    mark_needs_measure();
    mark_needs_layout();
    mark_needs_paint();
}

void ColumnBrowserWidget::set_sort(SortCriteria criteria) {
    if (m_sort_criteria == criteria) {
        m_sort_direction = (m_sort_direction == SortDirection::Ascending) ? SortDirection::Descending : SortDirection::Ascending;
    } else {
        m_sort_criteria = criteria;
        m_sort_direction = SortDirection::Ascending;
    }
    FileModel::sort_items(m_current_items, m_sort_criteria, m_sort_direction);
    mark_needs_paint();
}

void ColumnBrowserWidget::update_inspector_from_path(const std::filesystem::path& path) {
    m_selected_path = path;
    if (path.empty()) {
        m_has_selection = false;
        m_inspector_name = m_current_root.filename().string();
        if (m_inspector_name.empty()) m_inspector_name = "Root";
        m_inspector_type = "Directory";
        m_inspector_path = m_current_root.string();
        m_inspector_size = std::to_string(m_current_items.size()) + " items";
        m_inspector_perms = "Directory";
        m_inspector_icon = txui::IconType::Folder;
        mark_needs_paint();
        return;
    }

    m_has_selection = true;
    auto item = FileModel::stat_file(path);
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
    rebuild_breadcrumbs();
    mark_needs_paint();
}

void ColumnBrowserWidget::on_item_selected(size_t col_index, size_t item_index) {
    if (m_model.select_item(col_index, item_index)) {
        rebuild_columns();
    }
    if (col_index < m_model.columns().size()) {
        const auto& col = m_model.columns()[col_index];
        if (item_index < col.items.size()) {
            m_selected_path = col.items[item_index].path;
            update_inspector_from_path(m_selected_path);
        }
    }
    rebuild_breadcrumbs();
    mark_needs_paint();
}

void ColumnBrowserWidget::on_item_double_clicked(size_t col_index, size_t item_index) {
    if (col_index < m_model.columns().size()) {
        const auto& col = m_model.columns()[col_index];
        if (item_index < col.items.size()) {
            const auto& item = col.items[item_index];
            if (item.type == FileType::Directory) {
                navigate_to(item.path);
            } else if (m_on_execute) {
                RecentManager::instance().add_recent(item.path);
                m_on_execute(item.path);
            }
        }
    }
}

void ColumnBrowserWidget::open_quick_look(const std::filesystem::path& path) {
    m_quick_look_path = path;
    m_quick_look_lines.clear();
    m_quick_look_open = true;

    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec)) {
        std::string ext = path.extension().string();
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (ext == ".txt" || ext == ".md" || ext == ".cpp" || ext == ".h" || ext == ".hpp" ||
            ext == ".py" || ext == ".sh" || ext == ".toml" || ext == ".json" || ext == ".conf" || ext == ".log") {
            std::ifstream in(path);
            std::string line;
            size_t count = 0;
            while (std::getline(in, line) && count < 40) {
                m_quick_look_lines.push_back(line);
                count++;
            }
        }
    }
    RecentManager::instance().add_recent(path);
    mark_needs_paint();
}

void ColumnBrowserWidget::close_quick_look() {
    m_quick_look_open = false;
    m_quick_look_lines.clear();
    mark_needs_paint();
}

void ColumnBrowserWidget::open_context_menu(double x, double y, const std::filesystem::path& path, bool is_background) {
    m_context_menu_open = true;
    m_context_menu_is_background = is_background;
    m_context_menu_pos = txui::Point(x, y);
    m_context_menu_path = path;
    mark_needs_paint();
}

void ColumnBrowserWidget::close_context_menu() {
    m_context_menu_open = false;
    m_context_menu_is_background = false;
    mark_needs_paint();
}

void ColumnBrowserWidget::show_toast(const std::string& msg) const {
    m_toast_message = msg;
    m_toast_time = std::chrono::steady_clock::now();
    const_cast<ColumnBrowserWidget*>(this)->mark_needs_paint();
}

void ColumnBrowserWidget::do_new_folder() {
    auto res = FileOperations::create_folder(m_current_root);
    if (res.success) {
        show_toast("Created: " + res.resulting_path.filename().string());
        refresh_current_directory();
        if (m_view_mode == ViewMode::Column) {
            rebuild_columns();
        }
        update_inspector_from_path(res.resulting_path);
    } else {
        show_toast("Failed: " + res.error_message);
    }
}

void ColumnBrowserWidget::do_new_file() {
    auto res = FileOperations::create_file(m_current_root);
    if (res.success) {
        show_toast("Created: " + res.resulting_path.filename().string());
        refresh_current_directory();
        if (m_view_mode == ViewMode::Column) {
            rebuild_columns();
        }
        update_inspector_from_path(res.resulting_path);
    } else {
        show_toast("Failed: " + res.error_message);
    }
}

void ColumnBrowserWidget::do_copy() {
    auto p = get_selected_path();
    if (!p.empty()) {
        FileClipboard::instance().copy_single(p);
        show_toast("Copied: " + p.filename().string());
    }
}

void ColumnBrowserWidget::do_cut() {
    auto p = get_selected_path();
    if (!p.empty()) {
        FileClipboard::instance().cut_single(p);
        show_toast("Cut: " + p.filename().string());
    }
}

void ColumnBrowserWidget::do_paste() {
    if (!FileClipboard::instance().has_items()) {
        show_toast("Clipboard is empty");
        return;
    }
    auto results = FileClipboard::instance().paste_into(m_current_root);
    size_t count = 0;
    std::string err;
    for (const auto& r : results) {
        if (r.success) count++;
        else err = r.error_message;
    }
    if (count > 0) {
        show_toast("Pasted " + std::to_string(count) + " item(s)");
        refresh_current_directory();
        if (m_view_mode == ViewMode::Column) {
            rebuild_columns();
        }
    } else if (!err.empty()) {
        show_toast("Paste error: " + err);
    }
}

void ColumnBrowserWidget::do_trash() {
    auto p = get_selected_path();
    if (!p.empty()) {
        auto res = FileOperations::trash_path(p);
        if (res.success) {
            show_toast("Moved to Trash: " + p.filename().string());
            refresh_current_directory();
            if (m_view_mode == ViewMode::Column) {
                rebuild_columns();
            }
        } else {
            show_toast("Trash error: " + res.error_message);
        }
    }
}

void ColumnBrowserWidget::do_rename() {
    auto p = get_selected_path();
    if (p.empty()) return;

    std::string stem = p.stem().string();
    std::string ext = p.extension().string();
    std::string new_name = stem + " (Renamed)" + ext;
    auto res = FileOperations::rename_path(p, new_name);
    if (res.success) {
        show_toast("Renamed to: " + new_name);
        refresh_current_directory();
        if (m_view_mode == ViewMode::Column) {
            rebuild_columns();
        }
        update_inspector_from_path(res.resulting_path);
    } else {
        show_toast("Rename error: " + res.error_message);
    }
}

std::filesystem::path ColumnBrowserWidget::get_selected_path() const {
    if (m_view_mode == ViewMode::Column) {
        if (m_model.columns().empty()) return m_current_root;
        const auto& last_col = m_model.columns().back();
        if (last_col.selected_index >= 0 && last_col.selected_index < static_cast<int>(last_col.items.size())) {
            return last_col.items[static_cast<size_t>(last_col.selected_index)].path;
        }
        return last_col.directory_path;
    }
    return m_selected_path;
}

txui::Size ColumnBrowserWidget::measure_override(const txui::Constraints& constraints) noexcept {
    double toolbar_h = 44.0;
    double status_h  = 26.0;
    double middle_h  = std::max(0.0, constraints.max_height - toolbar_h - status_h);
    double sidebar_w = 170.0;
    double inspector_w = (m_view_mode == ViewMode::Column) ? 230.0 : 0.0;
    double content_w = std::max(0.0, constraints.max_width - sidebar_w - inspector_w);

    if (m_view_mode == ViewMode::Column) {
        txui::Constraints scroll_c(content_w, content_w, middle_h, middle_h);
        m_scroll_area->measure(scroll_c);
    }

    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void ColumnBrowserWidget::layout_override(const txui::Rect& frame) noexcept {
    double toolbar_h = 44.0;
    double status_h  = 26.0;
    double middle_h  = std::max(0.0, frame.height() - toolbar_h - status_h);
    double sidebar_w = 170.0;
    double inspector_w = (m_view_mode == ViewMode::Column) ? 230.0 : 0.0;
    double content_w = std::max(0.0, frame.width() - sidebar_w - inspector_w);

    // 1. Toolbar buttons
    double btn_y = frame.top() + 8.0;
    double btn_size = 28.0;
    m_back_btn_rect = txui::Rect(frame.left() + 12.0, btn_y, btn_size, btn_size);
    m_fwd_btn_rect  = txui::Rect(frame.left() + 12.0 + btn_size + 6.0, btn_y, btn_size, btn_size);

    // 2. View mode switcher (4 segments: Grid, List, Column, Gallery)
    double vm_x = m_fwd_btn_rect.right() + 14.0;
    double seg_w = 28.0;
    m_view_grid_rect = txui::Rect(vm_x, btn_y, seg_w, btn_size);
    m_view_list_rect = txui::Rect(vm_x + seg_w, btn_y, seg_w, btn_size);
    m_view_col_rect  = txui::Rect(vm_x + seg_w * 2.0, btn_y, seg_w, btn_size);
    m_view_gal_rect  = txui::Rect(vm_x + seg_w * 3.0, btn_y, seg_w, btn_size);

    // New Item (+) button
    m_new_btn_rect = txui::Rect(vm_x + seg_w * 4.0 + 8.0, btn_y, btn_size, btn_size);

    // 3. Right Toolbar controls
    double right_x = frame.right() - 12.0;

    // More options (placeholder)
    m_more_btn_rect = txui::Rect(right_x - 26.0, btn_y + 1.0, 26.0, 26.0);
    right_x -= 32.0;

    // Share button (placeholder)
    m_share_btn_rect = txui::Rect(right_x - 26.0, btn_y + 1.0, 26.0, 26.0);
    right_x -= 34.0;

    // Search input field
    double search_w = 150.0;
    m_search_rect = txui::Rect(right_x - search_w, btn_y + 1.0, search_w, 26.0);
    m_search_clear_rect = txui::Rect(m_search_rect.right() - 20.0, btn_y + 4.0, 16.0, 18.0);
    right_x -= (search_w + 10.0);

    // Sort button
    double sort_w = 95.0;
    m_sort_btn_rect = txui::Rect(right_x - sort_w, btn_y + 1.0, sort_w, 26.0);
    right_x -= (sort_w + 12.0);

    // 4. Breadcrumbs (fits in remaining space between view switcher and sort button)
    double bc_start_x = m_new_btn_rect.right() + 14.0;
    double bc_max_w = std::max(60.0, right_x - bc_start_x);
    double cur_bc_x = bc_start_x;
    for (auto& bc : m_breadcrumbs) {
        double pill_w = static_cast<double>(bc.name.size()) * 7.0 + 16.0;
        if (cur_bc_x + pill_w > bc_start_x + bc_max_w) {
            bc.rect = txui::Rect{};
            continue;
        }
        bc.rect = txui::Rect(cur_bc_x, btn_y + 2.0, pill_w, 24.0);
        cur_bc_x += pill_w + 14.0;
    }

    // 5. Sidebar items layout
    double sb_y = frame.top() + toolbar_h + 10.0;
    for (auto& item : m_sidebar_items) {
        if (item.is_section_header) {
            item.rect = txui::Rect(frame.left() + 16.0, sb_y + 4.0, sidebar_w - 32.0, 18.0);
            sb_y += 24.0;
        } else {
            item.rect = txui::Rect(frame.left() + 8.0, sb_y, sidebar_w - 16.0, 28.0);
            sb_y += 32.0;
        }
    }

    // 6. Column view scroll area
    if (m_view_mode == ViewMode::Column) {
        m_scroll_area->layout(txui::Rect(frame.left() + sidebar_w, frame.top() + toolbar_h, content_w, middle_h));
        double insp_x = frame.left() + sidebar_w + content_w;
        double insp_y = frame.top() + toolbar_h;
        m_open_btn_rect  = txui::Rect(insp_x + 20.0, insp_y + middle_h - 75.0, inspector_w - 40.0, 30.0);
        m_trash_btn_rect = txui::Rect(insp_x + 20.0, insp_y + middle_h - 38.0, inspector_w - 40.0, 26.0);
    }
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
    const double inspector_w = (m_view_mode == ViewMode::Column) ? 230.0 : 0.0;
    const double content_w = std::max(0.0, W - sidebar_w - inspector_w);

    // 1. Background
    painter.fill_rect(frame(), txui::Color(26, 28, 38, 255));

    // 2. Toolbar
    paint_toolbar(painter);

    // 3. Sidebar
    paint_sidebar(painter);

    // 4. Content Area
    txui::Rect content_area(gx + sidebar_w, gy + toolbar_h, content_w, middle_h);
    painter.push_clip(content_area);

    switch (m_view_mode) {
        case ViewMode::IconGrid:
            paint_icon_grid(painter, content_area);
            painter.pop_clip();
            break;
        case ViewMode::List:
            paint_list_view(painter, content_area);
            painter.pop_clip();
            break;
        case ViewMode::Column: {
            m_scroll_area->paint(painter);
            painter.pop_clip();

            // Right Inspector panel for Column view (drawn unclipped)
            txui::Rect insp_rect(gx + sidebar_w + content_w, gy + toolbar_h, inspector_w, middle_h);
            paint_inspector(painter, insp_rect);
            break;
        }
        case ViewMode::Gallery:
            paint_gallery_view(painter, content_area);
            painter.pop_clip();
            break;
    }

    // 5. Status Bar
    paint_status_bar(painter);

    // 6. Quick Look Modal (if open)
    if (m_quick_look_open) {
        paint_quick_look(painter);
    }

    // 7. Context Menu (if open)
    if (m_context_menu_open) {
        paint_context_menu(painter);
    }
}

void ColumnBrowserWidget::paint_toolbar(txui::Painter& painter) const noexcept {
    const double gx = frame().left();
    const double gy = frame().top();
    const double W  = frame().width();
    const double toolbar_h = 44.0;

    // Toolbar background with bottom separator
    painter.fill_gradient_rect(
        txui::Rect(gx, gy, W, toolbar_h),
        txui::Color(34, 36, 50, 255),
        txui::Color(28, 30, 42, 255)
    );
    painter.fill_rect(txui::Rect(gx, gy + toolbar_h - 1.0, W, 1.0), txui::Color(48, 52, 70, 255));

    // Nav Buttons (<, >)
    auto draw_nav_btn = [&](const txui::Rect& r, const std::string& symbol, bool hovered, bool enabled) {
        txui::Color bg = hovered && enabled ? txui::Color(65, 70, 95, 255) : txui::Color(42, 45, 62, 255);
        txui::Color fg = enabled ? txui::Color(230, 235, 250, 255) : txui::Color(100, 105, 125, 180);
        painter.fill_rounded_rect(r, 6.0, bg);
        painter.draw_text(txui::Point(r.left() + (r.width() - 8.0)/2.0, r.top() + (r.height() - 14.0)/2.0), symbol, fg, 13.0, true);
    };

    draw_nav_btn(m_back_btn_rect, "<", m_back_hovered, m_history_idx > 0);
    draw_nav_btn(m_fwd_btn_rect,  ">", m_fwd_hovered,  m_history_idx + 1 < m_history.size());

    // View Mode Switcher Pill
    txui::Rect vm_full(m_view_grid_rect.left(), m_view_grid_rect.top(), m_view_gal_rect.right() - m_view_grid_rect.left(), 28.0);
    painter.fill_rounded_rect(vm_full, 6.0, txui::Color(36, 38, 54, 255));

    auto draw_mode_seg = [&](const txui::Rect& r, const char* symbol, bool active, bool hovered) {
        if (active) {
            painter.fill_rounded_rect(r, 5.0, txui::Color(55, 125, 245, 255));
            painter.draw_text(txui::Point(r.left() + (r.width() - 10.0)/2.0, r.top() + 6.0), symbol, txui::Color(255, 255, 255, 255), 11.0, true);
        } else {
            if (hovered) painter.fill_rounded_rect(r, 5.0, txui::Color(255, 255, 255, 18));
            painter.draw_text(txui::Point(r.left() + (r.width() - 10.0)/2.0, r.top() + 6.0), symbol, txui::Color(160, 165, 185, 240), 11.0, false);
        }
    };

    draw_mode_seg(m_view_grid_rect, "::", m_view_mode == ViewMode::IconGrid, m_view_grid_hovered);
    draw_mode_seg(m_view_list_rect, "=",  m_view_mode == ViewMode::List,     m_view_list_hovered);
    draw_mode_seg(m_view_col_rect,  "||", m_view_mode == ViewMode::Column,   m_view_col_hovered);
    draw_mode_seg(m_view_gal_rect,  "#",  m_view_mode == ViewMode::Gallery,  m_view_gal_hovered);

    // New Action Button (+)
    txui::Color new_bg = m_new_btn_hovered ? txui::Color(55, 62, 85, 255) : txui::Color(38, 42, 58, 220);
    painter.fill_rounded_rect(m_new_btn_rect, 6.0, new_bg);
    painter.draw_text(txui::Point(m_new_btn_rect.left() + 9.5, m_new_btn_rect.top() + 4.5), "+", txui::Color(240, 245, 255, 255), 14.0, true);

    // Breadcrumbs
    for (size_t i = 0; i < m_breadcrumbs.size(); ++i) {
        const auto& bc = m_breadcrumbs[i];
        if (bc.rect.width() <= 0) continue;
        bool is_last = (i == m_breadcrumbs.size() - 1);
        txui::Color pill_bg = bc.hovered ? txui::Color(60, 65, 90, 255) : (is_last ? txui::Color(45, 50, 72, 200) : txui::Color(35, 38, 54, 160));
        txui::Color text_fg = is_last ? txui::Color(255, 255, 255, 255) : txui::Color(190, 195, 215, 230);

        painter.fill_rounded_rect(bc.rect, 5.0, pill_bg);
        painter.draw_text(txui::Point(bc.rect.left() + 8.0, bc.rect.top() + 5.0), bc.name, text_fg, 11.5, is_last);

        if (!is_last) {
            painter.draw_text(txui::Point(bc.rect.right() + 4.0, bc.rect.top() + 5.0), ">", txui::Color(110, 115, 135, 200), 11.0);
        }
    }

    // Sort Button
    txui::Color sort_bg = m_sort_hovered ? txui::Color(55, 60, 80, 255) : txui::Color(38, 42, 58, 255);
    painter.fill_rounded_rect(m_sort_btn_rect, 6.0, sort_bg);
    std::string sort_text = "Sort: ";
    switch (m_sort_criteria) {
        case SortCriteria::Name:         sort_text += "Name"; break;
        case SortCriteria::DateModified: sort_text += "Date"; break;
        case SortCriteria::Size:         sort_text += "Size"; break;
        case SortCriteria::Kind:         sort_text += "Kind"; break;
    }
    sort_text += (m_sort_direction == SortDirection::Ascending) ? " ^" : " v";
    painter.draw_text(txui::Point(m_sort_btn_rect.left() + 8.0, m_sort_btn_rect.top() + 6.0), sort_text, txui::Color(200, 205, 225, 255), 11.0);

    // Search Field
    txui::Color search_bg = m_search_focused ? txui::Color(45, 50, 70, 255) : txui::Color(35, 38, 52, 255);
    txui::Color search_border = m_search_focused ? txui::Color(60, 130, 250, 255) : txui::Color(55, 60, 80, 255);
    painter.fill_rounded_rect(m_search_rect, 6.0, search_border);
    painter.fill_rounded_rect(txui::Rect(m_search_rect.left() + 1.0, m_search_rect.top() + 1.0, m_search_rect.width() - 2.0, m_search_rect.height() - 2.0), 5.0, search_bg);

    // Magnifying glass icon symbol
    painter.draw_circle(txui::Point(m_search_rect.left() + 12.0, m_search_rect.top() + 12.0), 4.5, 1.2, txui::Color(140, 145, 165, 220));
    painter.draw_line(txui::Point(m_search_rect.left() + 15.5, m_search_rect.top() + 15.5), txui::Point(m_search_rect.left() + 19.0, m_search_rect.top() + 19.0), 1.2, txui::Color(140, 145, 165, 220));

    if (m_search_query.empty()) {
        painter.draw_text(txui::Point(m_search_rect.left() + 24.0, m_search_rect.top() + 6.0), "Search", txui::Color(120, 125, 145, 180), 11.5);
    } else {
        painter.draw_text(txui::Point(m_search_rect.left() + 24.0, m_search_rect.top() + 6.0), m_search_query, txui::Color(240, 245, 255, 255), 11.5, true);
        // Clear 'x' button
        painter.fill_circle(txui::Point(m_search_clear_rect.left() + 8.0, m_search_clear_rect.top() + 9.0), 6.5, txui::Color(65, 70, 90, 255));
        painter.draw_text(txui::Point(m_search_clear_rect.left() + 5.0, m_search_clear_rect.top() + 2.0), "x", txui::Color(200, 205, 225, 255), 10.0);
    }

    // Share & More options icons (clearly rendered as inert/stubs)
    painter.fill_rounded_rect(m_share_btn_rect, 5.0, txui::Color(35, 38, 52, 160));
    painter.draw_text(txui::Point(m_share_btn_rect.left() + 7.5, m_share_btn_rect.top() + 5.5), "^", txui::Color(110, 115, 135, 150), 11.0);

    painter.fill_rounded_rect(m_more_btn_rect, 5.0, txui::Color(35, 38, 52, 160));
    painter.draw_text(txui::Point(m_more_btn_rect.left() + 6.0, m_more_btn_rect.top() + 4.0), "...", txui::Color(110, 115, 135, 150), 12.0, true);
}

void ColumnBrowserWidget::paint_sidebar(txui::Painter& painter) const noexcept {
    const double gx = frame().left();
    const double gy = frame().top();
    const double toolbar_h = 44.0;
    const double status_h  = 26.0;
    const double middle_h  = std::max(0.0, frame().height() - toolbar_h - status_h);
    const double sidebar_w = 170.0;

    txui::Rect sidebar_rect(gx, gy + toolbar_h, sidebar_w, middle_h);
    painter.fill_rect(sidebar_rect, txui::Color(22, 24, 34, 255));
    painter.fill_rect(txui::Rect(sidebar_rect.right() - 1.0, sidebar_rect.top(), 1.0, sidebar_rect.height()), txui::Color(45, 48, 64, 255));

    for (const auto& item : m_sidebar_items) {
        if (item.is_section_header) {
            painter.draw_text(txui::Point(item.rect.left(), item.rect.top()), item.name, txui::Color(115, 122, 145, 255), 10.0, true);
            continue;
        }

        bool is_active = (m_current_root == item.path);
        if (is_active) {
            painter.fill_rounded_rect(item.rect, 6.0, txui::Color(45, 110, 225, 220));
            painter.fill_rounded_rect(txui::Rect(item.rect.left() + 2.0, item.rect.top() + 4.0, 3.0, item.rect.height() - 8.0), 1.5, txui::Color(255, 255, 255, 255));
        } else if (item.hovered) {
            painter.fill_rounded_rect(item.rect, 6.0, txui::Color(255, 255, 255, 14));
        }

        double icon_cx = item.rect.left() + 16.0;
        double icon_cy = item.rect.top() + item.rect.height() / 2.0;
        txui::Color icon_col = is_active ? txui::Color(255, 255, 255, 255) : txui::Color(130, 165, 245, 240);

        if (item.icon_type == txui::IconType::Home) {
            painter.fill_circle(txui::Point(icon_cx, icon_cy), 5.0, icon_col);
        } else if (item.icon_type == txui::IconType::Trash) {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 4.5, icon_cy - 4.5, 9.0, 9.0), 2.0, txui::Color(240, 80, 80, 240));
        } else {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 4.5, icon_cy - 4.5, 9.0, 9.0), 2.0, icon_col);
        }

        txui::Color text_col = is_active ? txui::Color(255, 255, 255, 255) : txui::Color(210, 215, 230, 240);
        painter.draw_text(txui::Point(item.rect.left() + 28.0, item.rect.top() + 7.0), item.name, text_col, 12.0, is_active);
    }
}

void ColumnBrowserWidget::paint_icon_grid(txui::Painter& painter, const txui::Rect& area) const noexcept {
    m_grid_rects.clear();
    auto items = get_display_items();

    if (items.empty()) {
        std::string empty_msg = m_search_query.empty() ? "Folder is empty" : "No matching items found";
        painter.draw_text(txui::Point(area.left() + 30.0, area.top() + 40.0), empty_msg, txui::Color(130, 135, 155, 200), 13.0);
        return;
    }

    const double cell_w = 106.0;
    const double cell_h = 112.0;
    const double pad_x = 16.0;
    const double pad_y = 16.0;

    int cols = std::max(1, static_cast<int>((area.width() - pad_x) / (cell_w + pad_x)));
    double start_x = area.left() + pad_x;
    double start_y = area.top() + pad_y - m_scroll_y;

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        int row = static_cast<int>(i) / cols;
        int col = static_cast<int>(i) % cols;

        double cx = start_x + col * (cell_w + pad_x);
        double cy = start_y + row * (cell_h + pad_y);
        txui::Rect cell(cx, cy, cell_w, cell_h);

        // Record hit rect
        m_grid_rects.push_back({i, cell, txui::Rect(cx + (cell_w - 72.0)/2.0, cy + 6.0, 72.0, 68.0), item.path});

        // Visible cull
        if (cell.bottom() < area.top() || cell.top() > area.bottom()) continue;

        bool is_selected = (static_cast<int32_t>(i) == m_selected_item_idx);
        if (is_selected) {
            painter.fill_rounded_rect(cell, 8.0, txui::Color(55, 120, 240, 70));
            // Inner border
            painter.fill_rounded_rect(txui::Rect(cell.left() + 1.0, cell.top() + 1.0, cell.width() - 2.0, cell.height() - 2.0), 7.0, txui::Color(55, 120, 240, 120));
        }

        // Thumbnail / Icon box
        txui::Rect thumb_box(cx + (cell_w - 72.0)/2.0, cy + 6.0, 72.0, 68.0);

        bool drew_image = false;
        if (item.mime_type.starts_with("image/")) {
            auto thumb = ThumbnailCache::instance().get_thumbnail(item.path, 72);
            if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
                // Background card for thumbnail
                painter.fill_rounded_rect(thumb_box, 6.0, txui::Color(20, 22, 30, 255));
                // Center thumbnail inside thumb_box
                double tx = thumb_box.left() + (thumb_box.width() - thumb->width) * 0.5;
                double ty = thumb_box.top() + (thumb_box.height() - thumb->height) * 0.5;
                painter.draw_image(txui::Rect(tx, ty, thumb->width, thumb->height), thumb->pixels, thumb->width, thumb->height);
                // Subtle border around real thumbnail
                painter.draw_circle(txui::Point(thumb_box.left() + 6.0, thumb_box.top() + 6.0), 0.0, 0.0, txui::Color(0,0,0,0)); // no-op
                drew_image = true;
            }
        }

        if (!drew_image) {
            // Draw stylized vector icons
            if (item.type == FileType::Directory) {
                // Folder icon
                painter.fill_rounded_rect(thumb_box, 6.0, txui::Color(45, 130, 240, 230));
                painter.fill_rounded_rect(txui::Rect(thumb_box.left() + 6.0, thumb_box.top() + 4.0, 26.0, 8.0), 3.0, txui::Color(70, 160, 255, 255));
                painter.fill_rounded_rect(txui::Rect(thumb_box.left() + 4.0, thumb_box.top() + 10.0, thumb_box.width() - 8.0, thumb_box.height() - 14.0), 5.0, txui::Color(35, 115, 225, 255));
            } else if (item.is_executable) {
                // Executable binary icon
                painter.fill_rounded_rect(thumb_box, 10.0, txui::Color(45, 55, 75, 240));
                painter.fill_circle(txui::Point(thumb_box.left() + thumb_box.width()/2.0, thumb_box.top() + thumb_box.height()/2.0), 18.0, txui::Color(255, 135, 45, 240));
                painter.draw_text(txui::Point(thumb_box.left() + thumb_box.width()/2.0 - 5.0, thumb_box.top() + thumb_box.height()/2.0 - 7.0), "!", txui::Color(255, 255, 255, 255), 14.0, true);
            } else if (item.mime_type == "application/archive") {
                // Archive icon
                painter.fill_rounded_rect(thumb_box, 6.0, txui::Color(180, 110, 45, 240));
                painter.fill_rect(txui::Rect(thumb_box.left() + thumb_box.width()/2.0 - 3.0, thumb_box.top() + 6.0, 6.0, thumb_box.height() - 12.0), txui::Color(245, 185, 45, 240));
            } else {
                // Regular document file
                painter.fill_rounded_rect(thumb_box, 6.0, txui::Color(48, 52, 70, 220));
                painter.fill_rect(txui::Rect(thumb_box.left() + 14.0, thumb_box.top() + 18.0, thumb_box.width() - 28.0, 2.5), txui::Color(160, 165, 185, 200));
                painter.fill_rect(txui::Rect(thumb_box.left() + 14.0, thumb_box.top() + 26.0, thumb_box.width() - 28.0, 2.5), txui::Color(160, 165, 185, 200));
                painter.fill_rect(txui::Rect(thumb_box.left() + 14.0, thumb_box.top() + 34.0, thumb_box.width() - 36.0, 2.5), txui::Color(160, 165, 185, 200));
            }
        }

        // Tag dot on top-right of cell if tagged
        auto tag = TagManager::instance().get_tag(item.path);
        if (tag.has_value()) {
            txui::Color tc = TagManager::tag_color(tag.value());
            painter.fill_circle(txui::Point(thumb_box.right() - 4.0, thumb_box.top() + 4.0), 4.5, tc);
        }

        // Truncated item label below
        std::string label_text = item.name;
        if (label_text.size() > 13) {
            label_text = label_text.substr(0, 10) + "...";
        }
        txui::Color text_col = is_selected ? txui::Color(255, 255, 255, 255) : txui::Color(215, 220, 235, 240);
        double text_w = static_cast<double>(label_text.size()) * 6.5;
        double text_x = cx + (cell_w - text_w) * 0.5;
        painter.draw_text(txui::Point(text_x, cy + 82.0), label_text, text_col, 11.5, is_selected);
    }
}

void ColumnBrowserWidget::paint_list_view(txui::Painter& painter, const txui::Rect& area) const noexcept {
    m_list_rects.clear();
    auto items = get_display_items();

    // 1. Column Headers (height 28px)
    txui::Rect header_bar(area.left(), area.top(), area.width(), 28.0);
    painter.fill_rect(header_bar, txui::Color(32, 35, 48, 255));
    painter.fill_rect(txui::Rect(header_bar.left(), header_bar.bottom() - 1.0, header_bar.width(), 1.0), txui::Color(48, 52, 72, 255));

    double name_w = area.width() * 0.45;
    double date_w = 140.0;
    double size_w = 85.0;
    double kind_w = area.width() - name_w - date_w - size_w;

    auto draw_header_col = [&](double x, double w, const char* title, SortCriteria crit) {
        bool is_active = (m_sort_criteria == crit);
        std::string t = title;
        if (is_active) {
            t += (m_sort_direction == SortDirection::Ascending) ? " ^" : " v";
        }
        txui::Color fg = is_active ? txui::Color(255, 255, 255, 255) : txui::Color(140, 145, 168, 220);
        painter.draw_text(txui::Point(x + 10.0, header_bar.top() + 7.0), t, fg, 11.0, is_active);
        painter.fill_rect(txui::Rect(x + w - 1.0, header_bar.top() + 4.0, 1.0, 20.0), txui::Color(45, 48, 64, 200));
    };

    draw_header_col(area.left(), name_w, "Name", SortCriteria::Name);
    draw_header_col(area.left() + name_w, date_w, "Date Modified", SortCriteria::DateModified);
    draw_header_col(area.left() + name_w + date_w, size_w, "Size", SortCriteria::Size);
    draw_header_col(area.left() + name_w + date_w + size_w, kind_w, "Kind", SortCriteria::Kind);

    // 2. Table Rows (height 28px)
    double row_h = 28.0;
    double cur_y = header_bar.bottom() - m_scroll_y;

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        txui::Rect row_rect(area.left(), cur_y, area.width(), row_h);
        m_list_rects.push_back({i, row_rect, item.path});

        // Visible cull
        if (row_rect.bottom() < header_bar.bottom() || row_rect.top() > area.bottom()) {
            cur_y += row_h;
            continue;
        }

        bool is_selected = (static_cast<int32_t>(i) == m_selected_item_idx);
        if (is_selected) {
            painter.fill_rect(row_rect, txui::Color(45, 110, 225, 220));
        } else if (i % 2 == 1) {
            painter.fill_rect(row_rect, txui::Color(255, 255, 255, 6));
        }

        // Icon miniature
        double icon_cx = row_rect.left() + 18.0;
        double icon_cy = row_rect.top() + row_h / 2.0;
        txui::Color icon_col = is_selected ? txui::Color(255, 255, 255, 255) : txui::Color(120, 160, 245, 240);

        if (item.type == FileType::Directory) {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 6.0, icon_cy - 5.0, 12.0, 10.0), 2.0, icon_col);
        } else if (item.mime_type.starts_with("image/")) {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 6.0, icon_cy - 6.0, 12.0, 12.0), 2.0, txui::Color(40, 180, 185, 255));
        } else if (item.is_executable) {
            painter.fill_circle(txui::Point(icon_cx, icon_cy), 5.5, txui::Color(255, 140, 45, 255));
        } else {
            painter.fill_rounded_rect(txui::Rect(icon_cx - 5.0, icon_cy - 6.0, 10.0, 12.0), 1.5, txui::Color(160, 165, 185, 220));
        }

        // Tag dot if tagged
        auto tag = TagManager::instance().get_tag(item.path);
        if (tag.has_value()) {
            painter.fill_circle(txui::Point(row_rect.left() + 32.0, icon_cy), 3.5, TagManager::tag_color(tag.value()));
        }

        // Name
        double name_x = row_rect.left() + (tag.has_value() ? 42.0 : 34.0);
        txui::Color text_fg = is_selected ? txui::Color(255, 255, 255, 255) : txui::Color(225, 230, 245, 240);
        std::string disp_name = item.name;
        if (disp_name.size() > 38) disp_name = disp_name.substr(0, 35) + "...";
        painter.draw_text(txui::Point(name_x, row_rect.top() + 7.0), disp_name, text_fg, 12.0, is_selected);

        // Date Modified
        txui::Color sec_fg = is_selected ? txui::Color(240, 245, 255, 220) : txui::Color(140, 145, 165, 200);
        painter.draw_text(txui::Point(area.left() + name_w + 10.0, row_rect.top() + 7.0), format_file_time(item.last_modified), sec_fg, 11.5);

        // Size
        std::string size_str = (item.type == FileType::Directory) ? "--" : format_file_size(item.size_bytes);
        painter.draw_text(txui::Point(area.left() + name_w + date_w + 10.0, row_rect.top() + 7.0), size_str, sec_fg, 11.5);

        // Kind
        std::string kind_str = (item.type == FileType::Directory) ? "Folder" : item.mime_type;
        if (kind_str.size() > 20) kind_str = kind_str.substr(0, 18) + "..";
        painter.draw_text(txui::Point(area.left() + name_w + date_w + size_w + 10.0, row_rect.top() + 7.0), kind_str, sec_fg, 11.5);

        cur_y += row_h;
    }
}

void ColumnBrowserWidget::paint_gallery_view(txui::Painter& painter, const txui::Rect& area) const noexcept {
    m_gallery_strip_rects.clear();
    auto items = get_display_items();

    double top_h = area.height() * 0.65;
    double bottom_h = area.height() - top_h;

    txui::Rect top_area(area.left(), area.top(), area.width(), top_h);
    txui::Rect bottom_area(area.left(), area.top() + top_h, area.width(), bottom_h);

    // Divider line between preview and filmstrip
    painter.fill_rect(txui::Rect(area.left(), top_area.bottom(), area.width(), 1.0), txui::Color(45, 48, 65, 255));

    // 1. Large Top Preview
    if (m_selected_item_idx >= 0 && static_cast<size_t>(m_selected_item_idx) < items.size()) {
        const auto& sel_item = items[static_cast<size_t>(m_selected_item_idx)];

        bool is_image = sel_item.mime_type.starts_with("image/");
        if (is_image) {
            uint32_t target_dim = static_cast<uint32_t>(std::min(top_h - 60.0, top_area.width() - 80.0));
            auto thumb = ThumbnailCache::instance().get_thumbnail(sel_item.path, target_dim);
            if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
                double px = top_area.left() + (top_area.width() - thumb->width) * 0.5;
                double py = top_area.top() + (top_h - 40.0 - thumb->height) * 0.5;
                // Soft card behind image
                painter.fill_rounded_rect(txui::Rect(px - 4.0, py - 4.0, thumb->width + 8.0, thumb->height + 8.0), 8.0, txui::Color(16, 18, 25, 255));
                painter.draw_image(txui::Rect(px, py, thumb->width, thumb->height), thumb->pixels, thumb->width, thumb->height);
            }
        } else {
            // Non-image large card
            double card_w = 320.0;
            double card_h = 160.0;
            double card_x = top_area.left() + (top_area.width() - card_w) * 0.5;
            double card_y = top_area.top() + (top_h - card_h) * 0.5 - 10.0;
            txui::Rect card(card_x, card_y, card_w, card_h);
            painter.fill_rounded_rect(card, 10.0, txui::Color(32, 35, 48, 255));

            painter.fill_circle(txui::Point(card.left() + 45.0, card.top() + 45.0), 20.0, txui::Color(55, 120, 240, 240));
            painter.draw_text(txui::Point(card.left() + 80.0, card.top() + 35.0), sel_item.name, txui::Color(255, 255, 255, 255), 14.0, true);
            painter.draw_text(txui::Point(card.left() + 80.0, card.top() + 55.0), format_file_size(sel_item.size_bytes), txui::Color(150, 155, 175, 220), 12.0);
            painter.draw_text(txui::Point(card.left() + 25.0, card.top() + 90.0), "Type: " + sel_item.mime_type, txui::Color(180, 185, 205, 220), 11.5);
            painter.draw_text(txui::Point(card.left() + 25.0, card.top() + 115.0), "Modified: " + format_file_time(sel_item.last_modified), txui::Color(180, 185, 205, 220), 11.5);
        }

        // File name & Tag below preview
        std::string cap = sel_item.name + " (" + format_file_size(sel_item.size_bytes) + ")";
        double cap_w = static_cast<double>(cap.size()) * 7.0;
        painter.draw_text(txui::Point(top_area.left() + (top_area.width() - cap_w)*0.5, top_area.bottom() - 25.0), cap, txui::Color(255, 255, 255, 255), 12.0, true);
    }

    // 2. Bottom Filmstrip (horizontal scrollable)
    double strip_pad_y = 12.0;
    double strip_cell_w = 70.0;
    double strip_cell_h = bottom_h - 24.0;
    double cur_x = bottom_area.left() + 16.0;

    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        txui::Rect cell(cur_x, bottom_area.top() + strip_pad_y, strip_cell_w, strip_cell_h);
        m_gallery_strip_rects.push_back({i, cell, cell, item.path});

        bool is_selected = (static_cast<int32_t>(i) == m_selected_item_idx);
        if (is_selected) {
            painter.fill_rounded_rect(cell, 6.0, txui::Color(55, 125, 245, 180));
        } else {
            painter.fill_rounded_rect(cell, 6.0, txui::Color(32, 35, 48, 160));
        }

        // Mini preview inside cell
        txui::Rect mini(cell.left() + 5.0, cell.top() + 5.0, cell.width() - 10.0, cell.height() - 24.0);
        if (item.mime_type.starts_with("image/")) {
            auto thumb = ThumbnailCache::instance().get_thumbnail(item.path, 50);
            if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
                double mx = mini.left() + (mini.width() - thumb->width)*0.5;
                double my = mini.top() + (mini.height() - thumb->height)*0.5;
                painter.draw_image(txui::Rect(mx, my, thumb->width, thumb->height), thumb->pixels, thumb->width, thumb->height);
            }
        } else {
            painter.fill_rounded_rect(txui::Rect(mini.left() + (mini.width() - 24.0)/2.0, mini.top() + 8.0, 24.0, 24.0), 4.0, txui::Color(55, 120, 240, 220));
        }

        // Mini label
        std::string mini_name = item.name;
        if (mini_name.size() > 8) mini_name = mini_name.substr(0, 6) + "..";
        painter.draw_text(txui::Point(cell.left() + 6.0, cell.bottom() - 16.0), mini_name, txui::Color(220, 225, 240, 255), 10.0);

        cur_x += strip_cell_w + 10.0;
    }
}

void ColumnBrowserWidget::paint_inspector(txui::Painter& painter, const txui::Rect& insp_rect) const noexcept {
    // 1. Background & separator
    painter.fill_rect(insp_rect, txui::Color(20, 22, 32, 255));
    painter.fill_rect(txui::Rect(insp_rect.left(), insp_rect.top(), 1.0, insp_rect.height()), txui::Color(45, 48, 64, 255));

    std::filesystem::path target = m_selected_path;
    if (target.empty() && !m_model.columns().empty()) {
        target = m_model.columns().back().directory_path;
    }

    std::error_code ec;
    bool is_dir = std::filesystem::is_directory(target, ec);
    std::string ext = target.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    bool is_image = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".svg");

    // 2. Preview Box
    double p_pad = 16.0;
    double p_w = insp_rect.width() - p_pad * 2.0;
    double p_h = 130.0;
    txui::Rect preview_box(insp_rect.left() + p_pad, insp_rect.top() + 16.0, p_w, p_h);

    painter.fill_rounded_rect(preview_box, 8.0, txui::Color(14, 16, 24, 255));
    painter.fill_rounded_rect(txui::Rect(preview_box.left() + 1.0, preview_box.top() + 1.0, preview_box.width() - 2.0, preview_box.height() - 2.0), 7.0, txui::Color(26, 28, 40, 255));

    if (is_image && std::filesystem::exists(target, ec)) {
        auto thumb = ThumbnailCache::instance().get_thumbnail(target, 120);
        if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
            double tx = preview_box.left() + (preview_box.width() - thumb->width) * 0.5;
            double ty = preview_box.top() + (preview_box.height() - thumb->height) * 0.5;
            painter.draw_image(txui::Rect(tx, ty, thumb->width, thumb->height), thumb->pixels, thumb->width, thumb->height);
        }
    } else if (is_dir) {
        double fcx = preview_box.left() + preview_box.width() / 2.0;
        double fcy = preview_box.top() + preview_box.height() / 2.0;
        painter.fill_rounded_rect(txui::Rect(fcx - 28.0, fcy - 18.0, 56.0, 38.0), 5.0, txui::Color(45, 130, 240, 240));
        painter.fill_rounded_rect(txui::Rect(fcx - 28.0, fcy - 22.0, 24.0, 8.0), 3.0, txui::Color(70, 160, 255, 255));
    } else {
        double fcx = preview_box.left() + preview_box.width() / 2.0;
        double fcy = preview_box.top() + preview_box.height() / 2.0;
        painter.fill_rounded_rect(txui::Rect(fcx - 20.0, fcy - 26.0, 40.0, 52.0), 4.0, txui::Color(55, 60, 80, 240));
        painter.fill_rect(txui::Rect(fcx - 12.0, fcy - 10.0, 24.0, 2.5), txui::Color(160, 165, 185, 200));
        painter.fill_rect(txui::Rect(fcx - 12.0, fcy - 2.0, 24.0, 2.5), txui::Color(160, 165, 185, 200));
        painter.fill_rect(txui::Rect(fcx - 12.0, fcy + 6.0, 16.0, 2.5), txui::Color(160, 165, 185, 200));
    }

    // 3. Name & Divider
    std::string disp_name = target.filename().string();
    if (disp_name.empty()) disp_name = target.string();
    if (disp_name.size() > 20) disp_name = disp_name.substr(0, 17) + "...";
    double cur_y = preview_box.bottom() + 14.0;
    painter.draw_text(txui::Point(insp_rect.left() + p_pad, cur_y), disp_name, txui::Color(255, 255, 255, 255), 13.0, true);

    cur_y += 22.0;
    painter.fill_rect(txui::Rect(insp_rect.left() + p_pad, cur_y, insp_rect.width() - p_pad * 2.0, 1.0), txui::Color(45, 48, 65, 200));

    // 4. Metadata Details
    cur_y += 10.0;
    auto draw_field = [&](const char* title, const std::string& val) {
        painter.draw_text(txui::Point(insp_rect.left() + p_pad, cur_y), title, txui::Color(120, 125, 145, 220), 10.0, true);
        std::string v = val;
        if (v.size() > 22) v = v.substr(0, 20) + "..";
        painter.draw_text(txui::Point(insp_rect.left() + p_pad, cur_y + 13.0), v, txui::Color(215, 220, 235, 240), 11.5);
        cur_y += 32.0;
    };

    if (is_dir) {
        draw_field("KIND", "Folder");
        auto items = FileModel::scan_directory(target, false);
        draw_field("SIZE", std::to_string(items.size()) + " items");
    } else {
        std::string kind = is_image ? ("Image (" + ext + ")") : "Document";
        draw_field("KIND", kind);
        auto sz = std::filesystem::file_size(target, ec);
        draw_field("SIZE", ec ? "--" : format_file_size(sz));
    }

    auto lwt = std::filesystem::last_write_time(target, ec);
    if (!ec) {
        draw_field("MODIFIED", format_file_time(lwt));
    }

    // 5. Action Buttons (at bottom)
    auto* self = const_cast<ColumnBrowserWidget*>(this);
    self->m_open_btn_rect  = txui::Rect(insp_rect.left() + 20.0, insp_rect.bottom() - 75.0, insp_rect.width() - 40.0, 30.0);
    self->m_trash_btn_rect = txui::Rect(insp_rect.left() + 20.0, insp_rect.bottom() - 38.0, insp_rect.width() - 40.0, 26.0);

    txui::Color open_bg = m_open_hovered ? txui::Color(65, 135, 255, 255) : txui::Color(45, 115, 240, 255);
    painter.fill_rounded_rect(m_open_btn_rect, 6.0, open_bg);
    painter.draw_text(txui::Point(m_open_btn_rect.left() + (m_open_btn_rect.width() - 34.0) / 2.0, m_open_btn_rect.top() + 7.5), "Open", txui::Color(255, 255, 255, 255), 13.0, true);

    if (!is_dir) {
        txui::Color trash_bg = m_trash_hovered ? txui::Color(80, 35, 45, 255) : txui::Color(45, 25, 32, 255);
        painter.fill_rounded_rect(m_trash_btn_rect, 6.0, trash_bg);
        painter.draw_text(txui::Point(m_trash_btn_rect.left() + (m_trash_btn_rect.width() - 85.0) / 2.0, m_trash_btn_rect.top() + 6.0), "Move to Trash", txui::Color(245, 120, 120, 230), 11.5);
    }
}

void ColumnBrowserWidget::paint_status_bar(txui::Painter& painter) const noexcept {
    const double gx = frame().left();
    const double gy = frame().top();
    const double W  = frame().width();
    const double H  = frame().height();
    const double status_h  = 26.0;

    txui::Rect status_rect(gx, gy + H - status_h, W, status_h);
    painter.fill_rect(status_rect, txui::Color(20, 21, 30, 255));
    painter.fill_rect(txui::Rect(gx, status_rect.top(), W, 1.0), txui::Color(40, 44, 58, 255));

    std::filesystem::path active_path = m_current_root;
    if (m_view_mode == ViewMode::Column) {
        if (!m_selected_path.empty()) {
            active_path = m_selected_path;
        } else if (!m_model.columns().empty()) {
            active_path = m_model.columns().back().directory_path;
        }
    } else {
        if (!m_selected_path.empty()) {
            active_path = m_selected_path;
        }
    }

    std::string path_msg = active_path.string();
    if (path_msg.size() > 70) path_msg = "..." + path_msg.substr(path_msg.size() - 67);
    painter.draw_text(txui::Point(gx + 12.0, status_rect.top() + 6.5), path_msg, txui::Color(145, 150, 170, 220), 11.5);

    std::string summary;
    if (m_view_mode == ViewMode::Column) {
        std::error_code ec;
        if (std::filesystem::is_directory(active_path, ec)) {
            auto dir_items = FileModel::scan_directory(active_path, false);
            summary = std::to_string(dir_items.size()) + " items";
        } else if (std::filesystem::exists(active_path, ec)) {
            summary = format_file_size(std::filesystem::file_size(active_path, ec));
        } else if (!m_model.columns().empty()) {
            summary = std::to_string(m_model.columns().back().items.size()) + " items";
        }
    } else {
        auto disp_items = get_display_items();
        summary = std::to_string(disp_items.size()) + " items";
        if (!m_search_query.empty()) {
            summary += " (filtered)";
        }
    }
    painter.draw_text(txui::Point(gx + W - 140.0, status_rect.top() + 6.5), summary, txui::Color(145, 150, 170, 220), 11.5);

    // Toast alert message in center
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_toast_time).count();
    if (!m_toast_message.empty() && elapsed < 4000) {
        double tw = static_cast<double>(m_toast_message.size()) * 7.5 + 24.0;
        double tx = gx + (W - tw) * 0.5;
        painter.fill_rounded_rect(txui::Rect(tx, status_rect.top() + 3.0, tw, 20.0), 10.0, txui::Color(45, 110, 225, 240));
        painter.draw_text(txui::Point(tx + 12.0, status_rect.top() + 5.0), m_toast_message, txui::Color(255, 255, 255, 255), 11.0, true);
    }
}

void ColumnBrowserWidget::paint_quick_look(txui::Painter& painter) const noexcept {
    const double W = frame().width();
    const double H = frame().height();

    // Dim background scrim
    painter.fill_rect(frame(), txui::Color(0, 0, 0, 140));

    double card_w = std::min(640.0, W - 40.0);
    double card_h = std::min(480.0, H - 40.0);
    double card_x = frame().left() + (W - card_w) * 0.5;
    double card_y = frame().top() + (H - card_h) * 0.5;
    const_cast<ColumnBrowserWidget*>(this)->m_quick_look_card_rect = txui::Rect(card_x, card_y, card_w, card_h);

    // Card background & shadow
    painter.fill_rounded_rect(txui::Rect(card_x - 2.0, card_y - 2.0, card_w + 4.0, card_h + 4.0), 12.0, txui::Color(60, 65, 85, 120));
    painter.fill_rounded_rect(txui::Rect(card_x, card_y, card_w, card_h), 12.0, txui::Color(28, 30, 42, 255));

    // Header bar
    painter.fill_rounded_rect(txui::Rect(card_x, card_y, card_w, 40.0), 12.0, txui::Color(35, 38, 52, 255));
    painter.fill_rect(txui::Rect(card_x, card_y + 39.0, card_w, 1.0), txui::Color(55, 58, 75, 255));

    // Close button (x)
    const_cast<ColumnBrowserWidget*>(this)->m_quick_look_close_rect = txui::Rect(card_x + card_w - 32.0, card_y + 10.0, 20.0, 20.0);
    painter.fill_circle(txui::Point(m_quick_look_close_rect.left() + 10.0, m_quick_look_close_rect.top() + 10.0), 9.0, txui::Color(55, 60, 80, 255));
    painter.draw_text(txui::Point(m_quick_look_close_rect.left() + 6.5, m_quick_look_close_rect.top() + 3.0), "x", txui::Color(220, 225, 240, 255), 11.0);

    // Title
    std::string ql_title = "Quick Look - " + m_quick_look_path.filename().string();
    painter.draw_text(txui::Point(card_x + 16.0, card_y + 12.0), ql_title, txui::Color(255, 255, 255, 255), 13.0, true);

    // Body content
    txui::Rect body(card_x + 16.0, card_y + 50.0, card_w - 32.0, card_h - 66.0);

    auto item = FileModel::stat_file(m_quick_look_path);
    if (item.mime_type.starts_with("image/")) {
        uint32_t max_d = static_cast<uint32_t>(std::min(body.width(), body.height()));
        auto thumb = ThumbnailCache::instance().get_thumbnail(m_quick_look_path, max_d);
        if (thumb && thumb->pixels && thumb->width > 0 && thumb->height > 0) {
            double ix = body.left() + (body.width() - thumb->width) * 0.5;
            double iy = body.top() + (body.height() - thumb->height) * 0.5;
            painter.draw_image(txui::Rect(ix, iy, thumb->width, thumb->height), thumb->pixels, thumb->width, thumb->height);
        }
    } else if (!m_quick_look_lines.empty()) {
        // Monospace text preview
        painter.fill_rounded_rect(body, 6.0, txui::Color(20, 22, 32, 255));
        double ty = body.top() + 8.0;
        for (const auto& line : m_quick_look_lines) {
            if (ty + 16.0 > body.bottom()) break;
            std::string l = line;
            if (l.size() > 75) l = l.substr(0, 72) + "...";
            painter.draw_mono_text(txui::Point(body.left() + 10.0, ty), l, txui::Color(210, 220, 240, 240), 11.0);
            ty += 16.0;
        }
    } else {
        // Binary / Generic card
        painter.fill_circle(txui::Point(body.left() + body.width()*0.5, body.top() + 70.0), 30.0, txui::Color(55, 120, 240, 240));
        std::string s_info = "Size: " + format_file_size(item.size_bytes) + "   Type: " + item.mime_type;
        double sw = static_cast<double>(s_info.size()) * 7.0;
        painter.draw_text(txui::Point(body.left() + (body.width() - sw)*0.5, body.top() + 120.0), s_info, txui::Color(210, 215, 235, 240), 12.0);
    }
}

void ColumnBrowserWidget::paint_context_menu(txui::Painter& painter) const noexcept {
    double mx = m_context_menu_pos.x;
    double my = m_context_menu_pos.y;
    double menu_w = 195.0;
    double menu_h = m_context_menu_is_background ? 120.0 : 255.0;

    // Clamp inside window
    if (mx + menu_w > frame().right()) mx = frame().right() - menu_w - 4.0;
    if (my + menu_h > frame().bottom()) my = frame().bottom() - menu_h - 4.0;

    txui::Rect menu_rect(mx, my, menu_w, menu_h);
    painter.fill_rounded_rect(txui::Rect(mx - 2.0, my - 2.0, menu_w + 4.0, menu_h + 4.0), 8.0, txui::Color(55, 60, 80, 100));
    painter.fill_rounded_rect(menu_rect, 8.0, txui::Color(32, 34, 46, 255));

    double opt_y = my + 8.0;
    double opt_h = 24.0;

    auto draw_opt = [&](txui::Rect& out_r, const char* label, bool enabled = true) {
        out_r = txui::Rect(mx + 6.0, opt_y, menu_w - 12.0, opt_h);
        txui::Color fg = enabled ? txui::Color(230, 235, 250, 255) : txui::Color(115, 120, 138, 180);
        painter.draw_text(txui::Point(out_r.left() + 10.0, out_r.top() + 5.0), label, fg, 11.5);
        opt_y += opt_h;
    };

    auto* self = const_cast<ColumnBrowserWidget*>(this);

    if (m_context_menu_is_background) {
        draw_opt(self->m_ctx_new_folder_rect, "New Folder");
        draw_opt(self->m_ctx_new_file_rect,   "New Document");
        bool can_paste = FileClipboard::instance().has_items();
        draw_opt(self->m_ctx_paste_rect,      "Paste", can_paste);
        draw_opt(self->m_ctx_info_rect,       "Refresh");
        return;
    }

    draw_opt(self->m_ctx_open_rect,      "Open");
    draw_opt(self->m_ctx_quicklook_rect, "Quick Look (Space)");
    draw_opt(self->m_ctx_rename_rect,    "Rename (F2)");
    draw_opt(self->m_ctx_copy_rect,      "Copy (Ctrl+C)");
    draw_opt(self->m_ctx_cut_rect,       "Cut (Ctrl+X)");
    draw_opt(self->m_ctx_trash_rect,     "Move to Trash");
    draw_opt(self->m_ctx_info_rect,      "Get Info");

    // Divider before tags
    painter.fill_rect(txui::Rect(mx + 8.0, opt_y + 2.0, menu_w - 16.0, 1.0), txui::Color(55, 58, 75, 255));
    opt_y += 6.0;

    // Tags label & dots
    painter.draw_text(txui::Point(mx + 16.0, opt_y), "Tags:", txui::Color(140, 145, 165, 220), 10.5, true);
    opt_y += 16.0;

    self->m_ctx_tag_rects.clear();
    const auto& tags = TagManager::available_tags();
    double dot_x = mx + 16.0;
    for (const auto& tag : tags) {
        txui::Rect dot_r(dot_x - 3.0, opt_y - 3.0, 18.0, 18.0);
        self->m_ctx_tag_rects.push_back({tag.name, dot_r});
        painter.fill_circle(txui::Point(dot_x + 6.0, opt_y + 6.0), 6.5, tag.color);
        dot_x += 20.0;
    }

    // Clear tag button
    self->m_ctx_clear_tag_rect = txui::Rect(mx + 16.0, opt_y + 20.0, menu_w - 32.0, 20.0);
    painter.draw_text(txui::Point(self->m_ctx_clear_tag_rect.left() + 4.0, self->m_ctx_clear_tag_rect.top() + 4.0), "Remove Tag", txui::Color(160, 165, 185, 200), 10.5);
}

bool ColumnBrowserWidget::handle_event(const txui::Event& event) noexcept {
    // 1. Keyboard Events
    if (event.type == txui::EventType::KeyDown) {
        if (event.keyboard.key == txui::Key::Space) {
            if (m_quick_look_open) {
                close_quick_look();
            } else {
                auto p = get_selected_path();
                if (!p.empty()) open_quick_look(p);
            }
            return true;
        }

        if (event.keyboard.key == txui::Key::Escape) {
            if (m_quick_look_open) {
                close_quick_look();
                return true;
            }
            if (m_context_menu_open) {
                close_context_menu();
                return true;
            }
            if (m_search_focused || !m_search_query.empty()) {
                m_search_query.clear();
                m_search_focused = false;
                mark_needs_paint();
                return true;
            }
        }

        if (event.keyboard.key == txui::Key::Backspace) {
            if (m_search_focused && !m_search_query.empty()) {
                m_search_query.pop_back();
                mark_needs_paint();
                return true;
            }
            auto path = get_selected_path();
            if (!path.empty() && m_on_trash) {
                m_on_trash(path);
                refresh_current_directory();
                return true;
            }
        }

        // Search text input
        if (m_search_focused) {
            if (event.keyboard.key == txui::Key::Enter) {
                m_search_focused = false;
                mark_needs_paint();
                return true;
            }

            char ch = 0;
            if (event.keyboard.key >= txui::Key::A && event.keyboard.key <= txui::Key::Z) {
                ch = 'a' + static_cast<char>(static_cast<uint16_t>(event.keyboard.key) - static_cast<uint16_t>(txui::Key::A));
            } else if (event.keyboard.key >= txui::Key::N0 && event.keyboard.key <= txui::Key::N9) {
                ch = '0' + static_cast<char>(static_cast<uint16_t>(event.keyboard.key) - static_cast<uint16_t>(txui::Key::N0));
            } else if (event.keyboard.key == txui::Key::Period) {
                ch = '.';
            } else if (event.keyboard.key == txui::Key::Minus) {
                ch = '-';
            }

            if (ch != 0) {
                m_search_query += ch;
                mark_needs_paint();
                return true;
            }
        } else {
            if (event.keyboard.key == txui::Key::N1) { set_view_mode(ViewMode::IconGrid); return true; }
            if (event.keyboard.key == txui::Key::N2) { set_view_mode(ViewMode::List);     return true; }
            if (event.keyboard.key == txui::Key::N3) { set_view_mode(ViewMode::Column);   return true; }
            if (event.keyboard.key == txui::Key::N4) { set_view_mode(ViewMode::Gallery);  return true; }
            if (event.keyboard.key == txui::Key::Slash) { m_search_focused = true; mark_needs_paint(); return true; }
            if (event.keyboard.key == txui::Key::S) {
                SortCriteria next = static_cast<SortCriteria>((static_cast<uint8_t>(m_sort_criteria) + 1) % 4);
                set_sort(next);
                return true;
            }
            if (event.keyboard.key == txui::Key::T) {
                auto p = get_selected_path();
                if (!p.empty()) {
                    auto cur = TagManager::instance().get_tag(p);
                    if (!cur.has_value()) {
                        TagManager::instance().set_tag(p, "Red");
                    } else if (cur.value() == "Red") {
                        TagManager::instance().set_tag(p, "Green");
                    } else {
                        TagManager::instance().remove_tag(p);
                    }
                    mark_needs_paint();
                    return true;
                }
            }
            if (event.keyboard.key == txui::Key::M) {
                auto p = get_selected_path();
                if (!p.empty()) {
                    open_context_menu(frame().left() + 340.0, frame().top() + 180.0, p);
                    return true;
                }
            }
            if (event.keyboard.key == txui::Key::N) {
                do_new_folder();
                return true;
            }
            if (event.keyboard.key == txui::Key::C) {
                do_copy();
                return true;
            }
            if (event.keyboard.key == txui::Key::X) {
                do_cut();
                return true;
            }
            if (event.keyboard.key == txui::Key::V) {
                do_paste();
                return true;
            }
            if (event.keyboard.key == txui::Key::Backspace) {
                do_trash();
                return true;
            }
            if (event.keyboard.key == txui::Key::F2) {
                do_rename();
                return true;
            }
            if (event.keyboard.key == txui::Key::A) {
                show_toast("Selected all items");
                return true;
            }
            if (event.keyboard.key == txui::Key::Right || event.keyboard.key == txui::Key::Down) {
                if (m_view_mode == ViewMode::Column) {
                    if (!m_model.columns().empty()) {
                        size_t active_col = 0;
                        for (size_t c = 0; c < m_model.columns().size(); ++c) {
                            if (m_model.columns()[c].selected_index >= 0) {
                                active_col = c;
                            }
                        }
                        const auto& col = m_model.columns()[active_col];
                        int next_idx = col.selected_index + 1;
                        if (next_idx < static_cast<int>(col.items.size())) {
                            on_item_selected(active_col, static_cast<size_t>(next_idx));
                            return true;
                        }
                    }
                } else {
                    auto items = get_display_items();
                    if (!items.empty()) {
                        m_selected_item_idx = std::min(static_cast<int32_t>(items.size() - 1), m_selected_item_idx + 1);
                        update_inspector_from_path(items[static_cast<size_t>(m_selected_item_idx)].path);
                        double visible_h = std::max(60.0, frame().height() - 72.0);
                        if (m_view_mode == ViewMode::List) {
                            double item_y = static_cast<double>(m_selected_item_idx) * 28.0;
                            if (item_y < m_scroll_y) {
                                m_scroll_y = item_y;
                            } else if (item_y + 28.0 > m_scroll_y + visible_h) {
                                m_scroll_y = item_y + 28.0 - visible_h;
                            }
                        } else if (m_view_mode == ViewMode::IconGrid) {
                            int cols = 5;
                            int row = m_selected_item_idx / cols;
                            double item_y = row * 124.0;
                            if (item_y < m_scroll_y) {
                                m_scroll_y = item_y;
                            } else if (item_y + 124.0 > m_scroll_y + visible_h) {
                                m_scroll_y = item_y + 124.0 - visible_h;
                            }
                        }
                        mark_needs_paint();
                        return true;
                    }
                }
            }
            if (event.keyboard.key == txui::Key::Left || event.keyboard.key == txui::Key::Up) {
                if (m_view_mode == ViewMode::Column) {
                    if (!m_model.columns().empty()) {
                        size_t active_col = 0;
                        for (size_t c = 0; c < m_model.columns().size(); ++c) {
                            if (m_model.columns()[c].selected_index >= 0) {
                                active_col = c;
                            }
                        }
                        const auto& col = m_model.columns()[active_col];
                        int prev_idx = col.selected_index - 1;
                        if (prev_idx >= 0) {
                            on_item_selected(active_col, static_cast<size_t>(prev_idx));
                            return true;
                        }
                    }
                } else {
                    auto items = get_display_items();
                    if (!items.empty()) {
                        m_selected_item_idx = std::max(0, m_selected_item_idx - 1);
                        update_inspector_from_path(items[static_cast<size_t>(m_selected_item_idx)].path);
                        double visible_h = std::max(60.0, frame().height() - 72.0);
                        if (m_view_mode == ViewMode::List) {
                            double item_y = static_cast<double>(m_selected_item_idx) * 28.0;
                            if (item_y < m_scroll_y) {
                                m_scroll_y = item_y;
                            } else if (item_y + 28.0 > m_scroll_y + visible_h) {
                                m_scroll_y = item_y + 28.0 - visible_h;
                            }
                        } else if (m_view_mode == ViewMode::IconGrid) {
                            int cols = 5;
                            int row = m_selected_item_idx / cols;
                            double item_y = row * 124.0;
                            if (item_y < m_scroll_y) {
                                m_scroll_y = item_y;
                            } else if (item_y + 124.0 > m_scroll_y + visible_h) {
                                m_scroll_y = item_y + 124.0 - visible_h;
                            }
                        }
                        mark_needs_paint();
                        return true;
                    }
                }
            }
        }
    }

    // 2. Mouse Scroll (for all views: Grid, List, Gallery, Column)
    if (event.type == txui::EventType::PointerScroll) {
        if (m_view_mode == ViewMode::Column) {
            // First let the specific column directly under the mouse pointer scroll vertically
            for (auto& child : m_columns_layout->children()) {
                if (child->handle_event(event)) {
                    mark_needs_paint();
                    return true;
                }
            }
            // If no column consumed the scroll, handle horizontal scrolling across columns
            if (m_scroll_area->handle_event(event)) {
                mark_needs_paint();
                return true;
            }
        } else {
            double max_scroll = 0.0;
            double visible_h = std::max(0.0, frame().height() - 70.0);
            auto items = get_display_items();
            if (m_view_mode == ViewMode::IconGrid) {
                const double cell_w = 106.0;
                const double pad_x = 16.0;
                double content_w = std::max(100.0, frame().width() - 170.0);
                int cols = std::max(1, static_cast<int>((content_w - pad_x) / (cell_w + pad_x)));
                double rows = std::ceil(static_cast<double>(items.size()) / static_cast<double>(cols));
                max_scroll = std::max(0.0, rows * 128.0 - visible_h);
            } else if (m_view_mode == ViewMode::List) {
                max_scroll = std::max(0.0, static_cast<double>(items.size()) * 28.0 - visible_h);
            } else if (m_view_mode == ViewMode::Gallery) {
                max_scroll = std::max(0.0, static_cast<double>(items.size()) * 92.0 - (frame().width() - 170.0));
            }
            double step = (std::abs(event.pointer.scroll_delta_y) < 5.0) 
                          ? event.pointer.scroll_delta_y * 24.0 
                          : event.pointer.scroll_delta_y * 3.0;
            m_scroll_y = std::clamp(m_scroll_y + step, 0.0, max_scroll);
            mark_needs_paint();
            return true;
        }
    }

    // 3. Pointer Move
    if (event.type == txui::EventType::PointerMove) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        m_back_hovered = m_back_btn_rect.contains(px, py);
        m_fwd_hovered  = m_fwd_btn_rect.contains(px, py);

        m_view_grid_hovered = m_view_grid_rect.contains(px, py);
        m_view_list_hovered = m_view_list_rect.contains(px, py);
        m_view_col_hovered  = m_view_col_rect.contains(px, py);
        m_view_gal_hovered  = m_view_gal_rect.contains(px, py);

        m_new_btn_hovered   = m_new_btn_rect.contains(px, py);

        m_sort_hovered   = m_sort_btn_rect.contains(px, py);
        m_search_hovered = m_search_rect.contains(px, py);

        for (auto& item : m_sidebar_items) {
            item.hovered = item.rect.contains(px, py);
        }
        for (auto& bc : m_breadcrumbs) {
            bc.hovered = bc.rect.contains(px, py);
        }

        mark_needs_paint();
    }

    // 4. Pointer Button Press (Click)
    if (event.type == txui::EventType::PointerButtonPress) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        // ── A. Right-Click Context Menu ──────────────────────────────────────────
        if (event.pointer.button == txui::MouseButton::Right) {
            bool hit_item = false;
            if (m_view_mode == ViewMode::IconGrid) {
                for (const auto& gi : m_grid_rects) {
                    if (gi.cell_rect.contains(px, py)) {
                        m_selected_item_idx = static_cast<int32_t>(gi.index);
                        update_inspector_from_path(gi.path);
                        open_context_menu(px, py, gi.path, false);
                        hit_item = true;
                        return true;
                    }
                }
            } else if (m_view_mode == ViewMode::List) {
                for (const auto& li : m_list_rects) {
                    if (li.row_rect.contains(px, py)) {
                        m_selected_item_idx = static_cast<int32_t>(li.index);
                        update_inspector_from_path(li.path);
                        open_context_menu(px, py, li.path, false);
                        hit_item = true;
                        return true;
                    }
                }
            } else if (m_view_mode == ViewMode::Gallery) {
                for (const auto& si : m_gallery_strip_rects) {
                    if (si.cell_rect.contains(px, py)) {
                        m_selected_item_idx = static_cast<int32_t>(si.index);
                        update_inspector_from_path(si.path);
                        open_context_menu(px, py, si.path, false);
                        hit_item = true;
                        return true;
                    }
                }
            } else if (m_view_mode == ViewMode::Column) {
                auto sel_path = get_selected_path();
                if (!sel_path.empty() && sel_path != m_current_root) {
                    open_context_menu(px, py, sel_path, false);
                    hit_item = true;
                    return true;
                }
            }

            if (!hit_item) {
                open_context_menu(px, py, m_current_root, true);
                return true;
            }
            return true;
        }

        // ── B. Context Menu Interactions ─────────────────────────────────────────
        if (m_context_menu_open) {
            if (m_context_menu_is_background) {
                if (m_ctx_new_folder_rect.contains(px, py)) {
                    do_new_folder();
                    close_context_menu();
                    return true;
                }
                if (m_ctx_new_file_rect.contains(px, py)) {
                    do_new_file();
                    close_context_menu();
                    return true;
                }
                if (m_ctx_paste_rect.contains(px, py)) {
                    do_paste();
                    close_context_menu();
                    return true;
                }
                if (m_ctx_info_rect.contains(px, py)) {
                    refresh_current_directory();
                    close_context_menu();
                    return true;
                }
                close_context_menu();
                return true;
            }

            // Item context menu
            if (m_ctx_open_rect.contains(px, py)) {
                if (std::filesystem::is_directory(m_context_menu_path)) {
                    navigate_to(m_context_menu_path);
                } else if (m_on_execute) {
                    RecentManager::instance().add_recent(m_context_menu_path);
                    m_on_execute(m_context_menu_path);
                }
                close_context_menu();
                return true;
            }
            if (m_ctx_quicklook_rect.contains(px, py)) {
                open_quick_look(m_context_menu_path);
                close_context_menu();
                return true;
            }
            if (m_ctx_rename_rect.contains(px, py)) {
                do_rename();
                close_context_menu();
                return true;
            }
            if (m_ctx_copy_rect.contains(px, py)) {
                FileClipboard::instance().copy_single(m_context_menu_path);
                show_toast("Copied: " + m_context_menu_path.filename().string());
                close_context_menu();
                return true;
            }
            if (m_ctx_cut_rect.contains(px, py)) {
                FileClipboard::instance().cut_single(m_context_menu_path);
                show_toast("Cut: " + m_context_menu_path.filename().string());
                close_context_menu();
                return true;
            }
            if (m_ctx_trash_rect.contains(px, py)) {
                do_trash();
                close_context_menu();
                return true;
            }
            if (m_ctx_info_rect.contains(px, py)) {
                update_inspector_from_path(m_context_menu_path);
                close_context_menu();
                return true;
            }
            for (const auto& [tag_name, tr] : m_ctx_tag_rects) {
                if (tr.contains(px, py)) {
                    TagManager::instance().set_tag(m_context_menu_path, tag_name);
                    close_context_menu();
                    return true;
                }
            }
            if (m_ctx_clear_tag_rect.contains(px, py)) {
                TagManager::instance().remove_tag(m_context_menu_path);
                close_context_menu();
                return true;
            }
            close_context_menu();
            return true;
        }

        // ── C. Quick Look Modal Interactions ─────────────────────────────────────
        if (m_quick_look_open) {
            if (m_quick_look_close_rect.contains(px, py) || !m_quick_look_card_rect.contains(px, py)) {
                close_quick_look();
                return true;
            }
            return true;
        }

        // ── D. Toolbar Navigation & View Mode Switcher ───────────────────────────
        if (m_back_btn_rect.contains(px, py)) {
            go_back();
            return true;
        }
        if (m_fwd_btn_rect.contains(px, py)) {
            go_forward();
            return true;
        }

        if (m_view_grid_rect.contains(px, py)) { set_view_mode(ViewMode::IconGrid); return true; }
        if (m_view_list_rect.contains(px, py)) { set_view_mode(ViewMode::List);     return true; }
        if (m_view_col_rect.contains(px, py))  { set_view_mode(ViewMode::Column);   return true; }
        if (m_view_gal_rect.contains(px, py))  { set_view_mode(ViewMode::Gallery);  return true; }

        if (m_new_btn_rect.contains(px, py)) {
            do_new_folder();
            return true;
        }

        if (m_sort_btn_rect.contains(px, py)) {
            // Cycle sort: Name -> Date -> Size -> Kind
            SortCriteria next = static_cast<SortCriteria>((static_cast<uint8_t>(m_sort_criteria) + 1) % 4);
            set_sort(next);
            return true;
        }

        // Search Field Click
        if (m_search_rect.contains(px, py)) {
            if (!m_search_query.empty() && m_search_clear_rect.contains(px, py)) {
                m_search_query.clear();
                m_search_focused = false;
            } else {
                m_search_focused = true;
            }
            mark_needs_paint();
            return true;
        } else {
            m_search_focused = false;
        }

        // Breadcrumbs Click
        for (const auto& bc : m_breadcrumbs) {
            if (bc.rect.contains(px, py)) {
                navigate_to(bc.path);
                return true;
            }
        }

        // Sidebar Item Click
        for (const auto& item : m_sidebar_items) {
            if (!item.is_section_header && item.rect.contains(px, py)) {
                if (item.name == "Recent (Session)") {
                    // Show recent items
                    const auto& rec = RecentManager::instance().recents();
                    m_current_items.clear();
                    for (const auto& p : rec) {
                        m_current_items.push_back(FileModel::stat_file(p));
                    }
                    m_selected_item_idx = m_current_items.empty() ? -1 : 0;
                    mark_needs_paint();
                } else {
                    navigate_to(item.path);
                }
                return true;
            }
        }

        // ── E. View Mode Content Item Clicks ─────────────────────────────────────
        if (m_view_mode == ViewMode::IconGrid) {
            for (const auto& gi : m_grid_rects) {
                if (gi.cell_rect.contains(px, py)) {
                    if (m_selected_item_idx == static_cast<int32_t>(gi.index)) {
                        // Double click / Enter
                        if (std::filesystem::is_directory(gi.path)) {
                            navigate_to(gi.path);
                        } else if (m_on_execute) {
                            RecentManager::instance().add_recent(gi.path);
                            m_on_execute(gi.path);
                        }
                    } else {
                        m_selected_item_idx = static_cast<int32_t>(gi.index);
                        update_inspector_from_path(gi.path);
                    }
                    return true;
                }
            }
        } else if (m_view_mode == ViewMode::List) {
            for (const auto& li : m_list_rects) {
                if (li.row_rect.contains(px, py)) {
                    if (m_selected_item_idx == static_cast<int32_t>(li.index)) {
                        if (std::filesystem::is_directory(li.path)) {
                            navigate_to(li.path);
                        } else if (m_on_execute) {
                            RecentManager::instance().add_recent(li.path);
                            m_on_execute(li.path);
                        }
                    } else {
                        m_selected_item_idx = static_cast<int32_t>(li.index);
                        update_inspector_from_path(li.path);
                    }
                    return true;
                }
            }
        } else if (m_view_mode == ViewMode::Gallery) {
            for (const auto& si : m_gallery_strip_rects) {
                if (si.cell_rect.contains(px, py)) {
                    m_selected_item_idx = static_cast<int32_t>(si.index);
                    update_inspector_from_path(si.path);
                    return true;
                }
            }
        }
    }

    if (m_view_mode == ViewMode::Column && m_scroll_area->handle_event(event)) {
        return true;
    }

    return false;
}

} // namespace tinexus::files::ui
