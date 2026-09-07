#include "ColumnBrowserWidget.hpp"
#include "files/file_operations.hpp"
#include "files/FileClipboard.hpp"
#include <txui/render/FontMetrics.hpp>
#include <algorithm>
#include <chrono>

namespace tinexus::files::ui {

ColumnBrowserWidget::ColumnBrowserWidget() {
    m_toolbar            = txui::make_ref<FileToolbarWidget>();
    m_sidebar            = txui::make_ref<FileSidebarWidget>();
    m_grid_view          = txui::make_ref<FileIconGridView>();
    m_list_view          = txui::make_ref<FileListView>();
    m_gallery_view       = txui::make_ref<FileGalleryView>();
    m_inspector          = txui::make_ref<FileInspectorWidget>();
    m_quick_look_modal   = txui::make_ref<FileQuickLookModal>();
    m_context_menu       = txui::make_ref<FileContextMenu>();

    m_scroll_area        = txui::make_ref<txui::ScrollArea>();
    m_scroll_area->set_allow_scroll_y(false);
    m_columns_layout     = txui::make_ref<txui::FlexLayout>();
    m_columns_layout->set_direction(txui::FlexDirection::Row);
    m_columns_layout->set_main_axis_alignment(txui::MainAxisAlignment::Start);
    m_columns_layout->set_cross_axis_alignment(txui::CrossAxisAlignment::Stretch);
    m_scroll_area->add_child(m_columns_layout);

    add_child(m_toolbar);
    add_child(m_sidebar);
    add_child(m_scroll_area);
    add_child(m_inspector);
    add_child(m_grid_view);
    add_child(m_list_view);
    add_child(m_gallery_view);
    add_child(m_quick_look_modal);
    add_child(m_context_menu);

    // Wire Toolbar Callbacks
    m_toolbar->on_back = [this]() { go_back(); };
    m_toolbar->on_forward = [this]() { go_forward(); };
    m_toolbar->on_view_mode_changed = [this](ViewMode m) { set_view_mode(m); };
    m_toolbar->on_new_item_clicked = [this]() { do_new_folder(); };
    m_toolbar->on_breadcrumb_clicked = [this](const std::filesystem::path& p) { navigate_to(p); };
    m_toolbar->on_search_changed = [this](const std::string&) { sync_active_view_items(); };
    m_toolbar->on_sort_clicked = [this]() {
        // Cycle criteria: Name -> DateModified -> Size -> Kind -> Name
        if (m_sort_criteria == SortCriteria::Name) set_sort(SortCriteria::DateModified);
        else if (m_sort_criteria == SortCriteria::DateModified) set_sort(SortCriteria::Size);
        else if (m_sort_criteria == SortCriteria::Size) set_sort(SortCriteria::Kind);
        else set_sort(SortCriteria::Name);
    };

    // Wire Sidebar Callbacks
    m_sidebar->on_location_selected = [this](const std::filesystem::path& p) { navigate_to(p); };

    // Common item activation logic
    auto on_item_act = [this](const FileItem& item) {
        if (item.type == FileType::Directory) {
            navigate_to(item.path);
        } else if (m_on_execute) {
            m_on_execute(item.path);
        }
    };

    auto on_item_sel = [this](const FileItem& item) {
        m_selected_path = item.path;
        m_inspector->inspect_path(item.path);
        mark_needs_paint();
    };

    auto on_ctx_menu = [this](double x, double y, const std::filesystem::path& p) {
        m_context_menu->show_at(x, y, p, p.empty());
    };

    // Wire Grid View
    m_grid_view->on_item_selected = [on_item_sel](int32_t, const FileItem& item) { on_item_sel(item); };
    m_grid_view->on_item_double_clicked = [on_item_act](int32_t, const FileItem& item) { on_item_act(item); };
    m_grid_view->on_context_menu = on_ctx_menu;

    // Wire List View
    m_list_view->on_item_selected = [on_item_sel](int32_t, const FileItem& item) { on_item_sel(item); };
    m_list_view->on_item_double_clicked = [on_item_act](int32_t, const FileItem& item) { on_item_act(item); };
    m_list_view->on_context_menu = on_ctx_menu;
    m_list_view->on_sort_changed = [this](SortCriteria c) {
        if (m_sort_criteria == c) {
            m_sort_direction = (m_sort_direction == SortDirection::Ascending) ? SortDirection::Descending : SortDirection::Ascending;
        } else {
            m_sort_criteria = c;
            m_sort_direction = SortDirection::Ascending;
        }
        m_toolbar->set_sort(m_sort_criteria, m_sort_direction);
        m_list_view->set_sort(m_sort_criteria, m_sort_direction);
        refresh_current_directory();
    };

    // Wire Gallery View
    m_gallery_view->on_item_selected = [on_item_sel](int32_t, const FileItem& item) { on_item_sel(item); };
    m_gallery_view->on_item_double_clicked = [on_item_act](int32_t, const FileItem& item) { on_item_act(item); };
    m_gallery_view->on_context_menu = on_ctx_menu;

    // Wire Inspector Actions
    m_inspector->on_open_requested = [this](const std::filesystem::path& p) {
        if (std::filesystem::is_directory(p)) {
            navigate_to(p);
        } else if (m_on_execute) {
            m_on_execute(p);
        }
    };
    m_inspector->on_trash_requested = [this](const std::filesystem::path& p) {
        if (m_on_trash) m_on_trash(p);
        refresh_current_directory();
    };

    // Wire Context Menu Actions
    m_context_menu->on_action_selected = [this](FileContextAction act, const std::filesystem::path& p) {
        switch (act) {
            case FileContextAction::Open:
                if (!p.empty()) {
                    if (std::filesystem::is_directory(p)) navigate_to(p);
                    else if (m_on_execute) m_on_execute(p);
                }
                break;
            case FileContextAction::QuickLook:
                if (!p.empty()) m_quick_look_modal->open_file(p);
                break;
            case FileContextAction::Rename:
                do_rename();
                break;
            case FileContextAction::Copy:
                do_copy();
                break;
            case FileContextAction::Cut:
                do_cut();
                break;
            case FileContextAction::Paste:
                do_paste();
                break;
            case FileContextAction::Trash:
                do_trash();
                break;
            case FileContextAction::NewFolder:
                do_new_folder();
                break;
            case FileContextAction::NewFile:
                do_new_file();
                break;
            case FileContextAction::Refresh:
                refresh_current_directory();
                break;
            case FileContextAction::ClearTag:
                if (!p.empty()) {
                    TagManager::instance().remove_tag(p);
                    mark_needs_paint();
                }
                break;
            case FileContextAction::GetInfo:
                m_inspector->inspect_path(p);
                break;
        }
    };

    m_context_menu->on_tag_selected = [this](const std::string& tag, const std::filesystem::path& p) {
        TagManager::instance().set_tag(p, tag);
        mark_needs_paint();
    };
}

void ColumnBrowserWidget::show_toast(const std::string& msg) const {
    m_toast_message = msg;
    m_toast_time = std::chrono::steady_clock::now();
    const_cast<ColumnBrowserWidget*>(this)->mark_needs_paint();
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

    m_toolbar->set_history_state(m_history_idx > 0, m_history_idx + 1 < m_history.size());
    m_sidebar->set_active_path(m_current_root);

    refresh_current_directory();
    update_breadcrumbs();

    if (m_view_mode == ViewMode::Column) {
        m_column_model.initialize(m_current_root);
        rebuild_columns();
    }
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
    if (m_view_mode != mode) {
        m_view_mode = mode;
        m_toolbar->set_view_mode(mode);
        if (m_view_mode == ViewMode::Column) {
            m_column_model.initialize(m_current_root);
            rebuild_columns();
        }
        sync_active_view_items();
        mark_needs_layout();
        mark_needs_paint();
    }
}

void ColumnBrowserWidget::set_sort(SortCriteria criteria) {
    if (m_sort_criteria == criteria) {
        m_sort_direction = (m_sort_direction == SortDirection::Ascending) ? SortDirection::Descending : SortDirection::Ascending;
    } else {
        m_sort_criteria = criteria;
        m_sort_direction = SortDirection::Ascending;
    }
    m_toolbar->set_sort(m_sort_criteria, m_sort_direction);
    m_list_view->set_sort(m_sort_criteria, m_sort_direction);
    refresh_current_directory();
}

std::filesystem::path ColumnBrowserWidget::get_selected_path() const {
    if (m_view_mode == ViewMode::Column) {
        if (m_column_model.columns().empty()) return m_current_root;
        const auto& last_col = m_column_model.columns().back();
        if (last_col.selected_index >= 0 && last_col.selected_index < static_cast<int>(last_col.items.size())) {
            return last_col.items[static_cast<size_t>(last_col.selected_index)].path;
        }
        return last_col.directory_path;
    }
    return m_selected_path;
}

void ColumnBrowserWidget::refresh_current_directory() {
    m_current_items = FileModel::scan_directory(m_current_root, false);
    FileModel::sort_items(m_current_items, m_sort_criteria, m_sort_direction);

    if (!m_current_items.empty()) {
        m_selected_path = m_current_items[0].path;
        m_inspector->inspect_path(m_selected_path);
    } else {
        m_selected_path.clear();
        m_inspector->inspect_path(m_current_root);
    }

    sync_active_view_items();
}

void ColumnBrowserWidget::sync_active_view_items() {
    const std::string& query = m_toolbar->search_query();
    std::vector<FileItem> display_items;

    if (query.empty()) {
        display_items = m_current_items;
    } else {
        std::string query_lower = query;
        std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        for (const auto& it : m_current_items) {
            std::string name_lower = it.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (name_lower.find(query_lower) != std::string::npos) {
                display_items.push_back(it);
            }
        }
    }

    m_grid_view->set_items(display_items);
    m_list_view->set_items(display_items);
    m_gallery_view->set_items(display_items);

    if (!m_selected_path.empty()) {
        m_grid_view->select_path(m_selected_path);
        m_list_view->select_path(m_selected_path);
        m_gallery_view->select_path(m_selected_path);
    }
}

void ColumnBrowserWidget::update_breadcrumbs() {
    std::vector<std::pair<std::string, std::filesystem::path>> crumbs;
    std::filesystem::path cur = m_current_root;

    std::vector<std::filesystem::path> parts;
    while (!cur.empty() && cur != cur.root_path()) {
        parts.push_back(cur);
        cur = cur.parent_path();
    }
    parts.push_back(m_current_root.root_path());
    std::reverse(parts.begin(), parts.end());

    for (const auto& p : parts) {
        std::string name = p.filename().string();
        if (name.empty() || p == p.root_path()) name = "Root";
        crumbs.push_back({name, p});
    }

    m_toolbar->set_breadcrumbs(crumbs);
}

void ColumnBrowserWidget::rebuild_columns() {
    m_columns_layout->remove_all_children();
    size_t col_index = 0;
    size_t total_cols = m_column_model.columns().size();

    for (const auto& level : m_column_model.columns()) {
        bool is_last = (col_index == total_cols - 1);
        auto col_widget = txui::make_ref<ColumnWidget>(level, col_index, is_last);

        col_widget->set_on_item_selected([this](size_t c, size_t i) {
            m_column_model.select_item(c, i);
            if (c < m_column_model.columns().size() && i < m_column_model.columns()[c].items.size()) {
                m_selected_path = m_column_model.columns()[c].items[i].path;
                m_inspector->inspect_path(m_selected_path);
            }
            rebuild_columns();
        });

        col_widget->set_on_item_double_clicked([this](size_t c, size_t i) {
            if (c < m_column_model.columns().size() && i < m_column_model.columns()[c].items.size()) {
                const auto& it = m_column_model.columns()[c].items[i];
                if (it.type == FileType::Directory) {
                    navigate_to(it.path);
                } else if (m_on_execute) {
                    m_on_execute(it.path);
                }
            }
        });

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

void ColumnBrowserWidget::do_new_folder() {
    auto res = FileOperations::create_folder(m_current_root, "New Folder");
    if (res.success) {
        show_toast("Created: " + res.resulting_path.filename().string());
        refresh_current_directory();
        if (m_view_mode == ViewMode::Column) rebuild_columns();
    } else {
        show_toast("Error: " + res.error_message);
    }
}

void ColumnBrowserWidget::do_new_file() {
    auto res = FileOperations::create_file(m_current_root, "New Document.txt");
    if (res.success) {
        show_toast("Created: " + res.resulting_path.filename().string());
        refresh_current_directory();
        if (m_view_mode == ViewMode::Column) rebuild_columns();
    } else {
        show_toast("Error: " + res.error_message);
    }
}

void ColumnBrowserWidget::do_copy() {
    auto p = get_selected_path();
    if (p.empty()) return;
    FileClipboard::instance().copy({p});
    show_toast("Copied to clipboard");
}

void ColumnBrowserWidget::do_cut() {
    auto p = get_selected_path();
    if (p.empty()) return;
    FileClipboard::instance().cut({p});
    show_toast("Cut to clipboard");
}

void ColumnBrowserWidget::do_paste() {
    if (!FileClipboard::instance().has_items()) return;
    auto src_paths = FileClipboard::instance().paths();
    bool is_cut = (FileClipboard::instance().mode() == ClipboardMode::Cut);

    for (const auto& src : src_paths) {
        auto res = is_cut ? FileOperations::move_path(src, m_current_root)
                          : FileOperations::copy_path(src, m_current_root);
        if (!res.success) {
            show_toast("Paste error: " + res.error_message);
            return;
        }
    }
    show_toast(is_cut ? "Moved items successfully" : "Pasted items successfully");
    FileClipboard::instance().clear();
    refresh_current_directory();
    if (m_view_mode == ViewMode::Column) rebuild_columns();
}

void ColumnBrowserWidget::do_trash() {
    auto p = get_selected_path();
    if (p.empty()) return;
    auto res = FileOperations::trash_path(p);
    if (res.success) {
        show_toast("Moved to trash");
        refresh_current_directory();
        if (m_view_mode == ViewMode::Column) rebuild_columns();
    } else {
        show_toast("Trash error: " + res.error_message);
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
        if (m_view_mode == ViewMode::Column) rebuild_columns();
        m_inspector->inspect_path(res.resulting_path);
    } else {
        show_toast("Rename error: " + res.error_message);
    }
}

txui::Size ColumnBrowserWidget::measure_override(const txui::Constraints& constraints) noexcept {
    const double toolbar_h = 44.0;
    const double status_h  = 26.0;
    const double middle_h  = std::max(0.0, constraints.max_height - toolbar_h - status_h);
    const double sidebar_w = 170.0;
    const double inspector_w = (m_view_mode == ViewMode::Column) ? 230.0 : 0.0;
    const double content_w = std::max(0.0, constraints.max_width - sidebar_w - inspector_w);

    m_toolbar->measure(txui::Constraints(constraints.max_width, constraints.max_width, toolbar_h, toolbar_h));
    m_sidebar->measure(txui::Constraints(sidebar_w, sidebar_w, middle_h, middle_h));

    if (m_view_mode == ViewMode::Column) {
        m_scroll_area->measure(txui::Constraints(content_w, content_w, middle_h, middle_h));
        m_inspector->measure(txui::Constraints(inspector_w, inspector_w, middle_h, middle_h));
    } else if (m_view_mode == ViewMode::IconGrid) {
        m_grid_view->measure(txui::Constraints(content_w, content_w, middle_h, middle_h));
    } else if (m_view_mode == ViewMode::List) {
        m_list_view->measure(txui::Constraints(content_w, content_w, middle_h, middle_h));
    } else if (m_view_mode == ViewMode::Gallery) {
        m_gallery_view->measure(txui::Constraints(content_w, content_w, middle_h, middle_h));
    }

    return constraints.constrain(txui::Size(constraints.max_width, constraints.max_height));
}

void ColumnBrowserWidget::layout_override(const txui::Rect& f) noexcept {
    const double toolbar_h = 44.0;
    const double status_h  = 26.0;
    const double middle_h  = std::max(0.0, f.height() - toolbar_h - status_h);
    const double sidebar_w = 170.0;
    const double inspector_w = (m_view_mode == ViewMode::Column) ? 230.0 : 0.0;
    const double content_w = std::max(0.0, f.width() - sidebar_w - inspector_w);

    m_toolbar->layout(txui::Rect(f.left(), f.top(), f.width(), toolbar_h));
    m_sidebar->layout(txui::Rect(f.left(), f.top() + toolbar_h, sidebar_w, middle_h));

    txui::Rect content_rect(f.left() + sidebar_w, f.top() + toolbar_h, content_w, middle_h);

    if (m_view_mode == ViewMode::Column) {
        m_scroll_area->layout(content_rect);
        m_inspector->layout(txui::Rect(f.left() + sidebar_w + content_w, f.top() + toolbar_h, inspector_w, middle_h));
    } else if (m_view_mode == ViewMode::IconGrid) {
        m_grid_view->layout(content_rect);
    } else if (m_view_mode == ViewMode::List) {
        m_list_view->layout(content_rect);
    } else if (m_view_mode == ViewMode::Gallery) {
        m_gallery_view->layout(content_rect);
    }

    m_quick_look_modal->layout(f);
    m_context_menu->layout(f);
}

void ColumnBrowserWidget::paint_override(txui::Painter& painter) const noexcept {
    const double gx = frame().left();
    const double gy = frame().top();
    const double W  = frame().width();
    const double H  = frame().height();
    const double status_h = 26.0;

    // 1. Overall Base Canvas
    painter.fill_rect(frame(), txui::Color(26, 28, 38, 255));

    // 2. Toolbar & Sidebar
    m_toolbar->paint(painter);
    m_sidebar->paint(painter);

    // 3. Active Content View
    if (m_view_mode == ViewMode::Column) {
        m_scroll_area->paint(painter);
        m_inspector->paint(painter);
    } else if (m_view_mode == ViewMode::IconGrid) {
        m_grid_view->paint(painter);
    } else if (m_view_mode == ViewMode::List) {
        m_list_view->paint(painter);
    } else if (m_view_mode == ViewMode::Gallery) {
        m_gallery_view->paint(painter);
    }

    // 4. Status Bar
    txui::Rect status_rect(gx, gy + H - status_h, W, status_h);
    painter.fill_rect(status_rect, txui::Color(20, 21, 30, 255));
    painter.fill_rect(txui::Rect(gx, status_rect.top(), W, 1.0), txui::Color(40, 44, 58, 255));

    std::filesystem::path active_path = get_selected_path();
    if (active_path.empty()) active_path = m_current_root;
    std::string path_msg = active_path.string();
    if (path_msg.size() > 70) path_msg = "..." + path_msg.substr(path_msg.size() - 67);
    painter.draw_text(txui::Point(gx + 12.0, status_rect.top() + 6.5), path_msg, txui::Color(145, 150, 170, 220), 11.5);

    std::string summary = std::to_string(m_current_items.size()) + " items";
    if (!m_toolbar->search_query().empty()) summary += " (filtered)";
    painter.draw_text(txui::Point(gx + W - 140.0, status_rect.top() + 6.5), summary, txui::Color(145, 150, 170, 220), 11.5);

    // Toast alert message in center
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_toast_time).count();
    if (!m_toast_message.empty() && elapsed < 4000) {
        auto tsz = txui::FontMetrics::measure(m_toast_message, 11.0, true);
        double tw = tsz.width + 24.0;
        double tx = gx + (W - tw) * 0.5;
        painter.fill_rounded_rect(txui::Rect(tx, status_rect.top() + 3.0, tw, 20.0), 10.0, txui::Color(45, 110, 225, 240));
        painter.draw_text(txui::Point(tx + 12.0, status_rect.top() + 5.0), m_toast_message, txui::Color(255, 255, 255, 255), 11.0, true);
    }

    // 5. Quick Look Modal (if open)
    if (m_quick_look_modal->is_open()) {
        m_quick_look_modal->paint(painter);
    }

    // 6. Context Menu (if open)
    if (m_context_menu->is_open()) {
        m_context_menu->paint(painter);
    }
}

bool ColumnBrowserWidget::handle_event(const txui::Event& event) noexcept {
    // 1. Modals capture all events first
    if (m_quick_look_modal->is_open()) {
        return m_quick_look_modal->handle_event(event);
    }
    if (m_context_menu->is_open()) {
        return m_context_menu->handle_event(event);
    }

    // 2. Global Keyboard Shortcuts
    if (event.type == txui::EventType::KeyDown) {
        if (event.keyboard.key == txui::Key::Space && !m_toolbar->is_search_focused()) {
            auto p = get_selected_path();
            if (!p.empty()) {
                m_quick_look_modal->open_file(p);
                return true;
            }
        }
        if (event.keyboard.key == txui::Key::F2) {
            do_rename();
            return true;
        }

        const bool ctrl = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Ctrl);
        if (ctrl) {
            if (event.keyboard.key == txui::Key::C) { do_copy(); return true; }
            if (event.keyboard.key == txui::Key::X) { do_cut(); return true; }
            if (event.keyboard.key == txui::Key::V) { do_paste(); return true; }
        }

        if (!m_toolbar->is_search_focused()) {
            if (event.keyboard.key == txui::Key::N1) { set_view_mode(ViewMode::IconGrid); return true; }
            if (event.keyboard.key == txui::Key::N2) { set_view_mode(ViewMode::List);     return true; }
            if (event.keyboard.key == txui::Key::N3) { set_view_mode(ViewMode::Column);   return true; }
            if (event.keyboard.key == txui::Key::N4) { set_view_mode(ViewMode::Gallery);  return true; }
            if (event.keyboard.key == txui::Key::Delete) { do_trash(); return true; }
        }
    }

    // 3. Child Widget Event Routing
    if (m_toolbar->handle_event(event)) return true;
    if (m_sidebar->handle_event(event)) return true;

    if (m_view_mode == ViewMode::Column) {
        if (m_inspector->handle_event(event)) return true;
        return m_scroll_area->handle_event(event);
    } else if (m_view_mode == ViewMode::IconGrid) {
        return m_grid_view->handle_event(event);
    } else if (m_view_mode == ViewMode::List) {
        return m_list_view->handle_event(event);
    } else if (m_view_mode == ViewMode::Gallery) {
        return m_gallery_view->handle_event(event);
    }

    return false;
}

} // namespace tinexus::files::ui
