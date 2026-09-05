#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/layout/ScrollArea.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/widgets/Label.hpp>
#include <txui/widgets/Icon.hpp>
#include <functional>
#include <vector>
#include <filesystem>
#include <optional>
#include "files/column_view_model.hpp"
#include "files/file_model.hpp"
#include "files/TagManager.hpp"
#include "files/ThumbnailCache.hpp"
#include "files/RecentManager.hpp"

namespace tinexus::files::ui {

enum class ViewMode : uint8_t {
    IconGrid = 0,
    List = 1,
    Column = 2,
    Gallery = 3
};

struct SidebarItem {
    std::string name;
    std::filesystem::path path;
    txui::IconType icon_type;
    txui::Rect rect{};
    bool hovered{false};
    bool is_section_header{false};
};

struct BreadcrumbPill {
    std::string name;
    std::filesystem::path path;
    txui::Rect rect{};
    bool hovered{false};
};

struct GridItemRect {
    size_t index{0};
    txui::Rect cell_rect{};
    txui::Rect thumb_rect{};
    std::filesystem::path path;
};

struct ListItemRect {
    size_t index{0};
    txui::Rect row_rect{};
    std::filesystem::path path;
};

class ColumnBrowserWidget : public txui::Widget {
private:
    txui::Ref<txui::ScrollArea> m_scroll_area;
    txui::Ref<txui::FlexLayout> m_columns_layout;

    ColumnViewModel m_model;
    std::filesystem::path m_current_root;
    std::vector<FileItem> m_current_items;

    ViewMode m_view_mode{ViewMode::IconGrid};
    SortCriteria m_sort_criteria{SortCriteria::Name};
    SortDirection m_sort_direction{SortDirection::Ascending};

    // Live search query
    std::string m_search_query{""};
    bool m_search_focused{false};

    // Selection
    int32_t m_selected_item_idx{-1};
    std::filesystem::path m_selected_path;

    // History stack for Back/Forward
    std::vector<std::filesystem::path> m_history;
    size_t m_history_idx{0};

    // Sidebar items grouped by section
    std::vector<SidebarItem> m_sidebar_items;

    // Breadcrumbs
    std::vector<BreadcrumbPill> m_breadcrumbs;

    // Layout hit-test rects
    txui::Rect m_back_btn_rect{};
    txui::Rect m_fwd_btn_rect{};
    bool m_back_hovered{false};
    bool m_fwd_hovered{false};

    // View mode switcher rects
    txui::Rect m_view_grid_rect{};
    txui::Rect m_view_list_rect{};
    txui::Rect m_view_col_rect{};
    txui::Rect m_view_gal_rect{};
    bool m_view_grid_hovered{false};
    bool m_view_list_hovered{false};
    bool m_view_col_hovered{false};
    bool m_view_gal_hovered{false};

    // Sort button & Search field rects
    txui::Rect m_sort_btn_rect{};
    txui::Rect m_search_rect{};
    txui::Rect m_search_clear_rect{};
    txui::Rect m_share_btn_rect{};
    txui::Rect m_more_btn_rect{};
    bool m_sort_hovered{false};
    bool m_search_hovered{false};
    bool m_share_hovered{false};
    bool m_more_hovered{false};

    // List view column header rects
    txui::Rect m_header_name_rect{};
    txui::Rect m_header_date_rect{};
    txui::Rect m_header_size_rect{};
    txui::Rect m_header_kind_rect{};

    // Scrolling in Grid & List views
    double m_scroll_y{0.0};
    double m_max_scroll_y{0.0};

    // Cached hit rects for current frame
    mutable std::vector<GridItemRect> m_grid_rects;
    mutable std::vector<ListItemRect> m_list_rects;
    mutable std::vector<GridItemRect> m_gallery_strip_rects;

    // Quick Look modal state
    bool m_quick_look_open{false};
    std::filesystem::path m_quick_look_path;
    txui::Rect m_quick_look_card_rect{};
    txui::Rect m_quick_look_close_rect{};
    std::vector<std::string> m_quick_look_lines;

    // Right-click context menu state
    bool m_context_menu_open{false};
    bool m_context_menu_is_background{false};
    txui::Point m_context_menu_pos{};
    std::filesystem::path m_context_menu_path;
    txui::Rect m_ctx_open_rect{};
    txui::Rect m_ctx_quicklook_rect{};
    txui::Rect m_ctx_copy_rect{};
    txui::Rect m_ctx_cut_rect{};
    txui::Rect m_ctx_paste_rect{};
    txui::Rect m_ctx_rename_rect{};
    txui::Rect m_ctx_trash_rect{};
    txui::Rect m_ctx_info_rect{};
    txui::Rect m_ctx_new_folder_rect{};
    txui::Rect m_ctx_new_file_rect{};
    std::vector<std::pair<std::string, txui::Rect>> m_ctx_tag_rects;
    txui::Rect m_ctx_clear_tag_rect{};

    // Toolbar New Action Button
    txui::Rect m_new_btn_rect{};
    bool m_new_btn_hovered{false};

    // User feedback toast alert
    mutable std::string m_toast_message{""};
    mutable std::chrono::steady_clock::time_point m_toast_time{};
    void show_toast(const std::string& msg) const;

    // Inspector Action Buttons (in Column view or inspector pane)
    txui::Rect m_open_btn_rect{};
    txui::Rect m_trash_btn_rect{};
    bool m_open_hovered{false};
    bool m_trash_hovered{false};

    // Cached inspector info
    std::string m_inspector_name{"No file selected"};
    std::string m_inspector_type{""};
    std::string m_inspector_size{""};
    std::string m_inspector_path{""};
    std::string m_inspector_perms{""};
    txui::IconType m_inspector_icon{txui::IconType::File};
    bool m_has_selection{false};

    std::function<void(const std::filesystem::path&)> m_on_execute;
    std::function<void(const std::filesystem::path&)> m_on_trash;
    std::function<void(const std::filesystem::path&)> m_on_uninstall;

    void refresh_current_directory();
    void rebuild_sidebar();
    void rebuild_breadcrumbs();
    void rebuild_columns();
    void update_inspector_from_path(const std::filesystem::path& path);
    void on_item_selected(size_t col_index, size_t item_index);
    void on_item_double_clicked(size_t col_index, size_t item_index);

    void open_quick_look(const std::filesystem::path& path);
    void close_quick_look();

    void open_context_menu(double x, double y, const std::filesystem::path& path, bool is_background = false);
    void close_context_menu();

    // Core File Operations
    void do_new_folder();
    void do_new_file();
    void do_copy();
    void do_cut();
    void do_paste();
    void do_trash();
    void do_rename();

    [[nodiscard]] std::vector<FileItem> get_display_items() const;

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

    void paint_toolbar(txui::Painter& painter) const noexcept;
    void paint_sidebar(txui::Painter& painter) const noexcept;
    void paint_icon_grid(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_list_view(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_gallery_view(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_status_bar(txui::Painter& painter) const noexcept;
    void paint_inspector(txui::Painter& painter, const txui::Rect& area) const noexcept;
    void paint_quick_look(txui::Painter& painter) const noexcept;
    void paint_context_menu(txui::Painter& painter) const noexcept;

public:
    ColumnBrowserWidget();
    ~ColumnBrowserWidget() override = default;

    void navigate_to(const std::filesystem::path& path, bool record_history = true);
    void go_back();
    void go_forward();
    void set_view_mode(ViewMode mode);
    void set_sort(SortCriteria criteria);

    void set_on_execute(std::function<void(const std::filesystem::path&)> callback) { m_on_execute = std::move(callback); }
    void set_on_trash(std::function<void(const std::filesystem::path&)> callback) { m_on_trash = std::move(callback); }
    void set_on_uninstall(std::function<void(const std::filesystem::path&)> callback) { m_on_uninstall = std::move(callback); }

    [[nodiscard]] std::filesystem::path get_selected_path() const;

    bool handle_event(const txui::Event& event) noexcept override;
};

} // namespace tinexus::files::ui
