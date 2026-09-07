#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/layout/ScrollArea.hpp>
#include <txui/layout/FlexLayout.hpp>
#include "files/column_view_model.hpp"
#include "files/file_model.hpp"
#include "files/TagManager.hpp"
#include "files/ThumbnailCache.hpp"
#include "files/RecentManager.hpp"
#include "files/ui/FileToolbarWidget.hpp"
#include "files/ui/FileSidebarWidget.hpp"
#include "files/ui/FileIconGridView.hpp"
#include "files/ui/FileListView.hpp"
#include "files/ui/FileGalleryView.hpp"
#include "files/ui/FileInspectorWidget.hpp"
#include "files/ui/FileQuickLookModal.hpp"
#include "files/ui/FileContextMenu.hpp"
#include "ColumnWidget.hpp"

#include <vector>
#include <filesystem>
#include <functional>
#include <chrono>

namespace tinexus::files::ui {

class ColumnBrowserWidget : public txui::Widget {
public:
    ColumnBrowserWidget();
    ~ColumnBrowserWidget() override = default;

    void navigate_to(const std::filesystem::path& path, bool record_history = true);
    void go_back();
    void go_forward();
    void set_view_mode(ViewMode mode);
    void set_sort(SortCriteria criteria);

    void set_on_execute(std::function<void(const std::filesystem::path&)> cb) { m_on_execute = std::move(cb); }
    void set_on_trash(std::function<void(const std::filesystem::path&)> cb) { m_on_trash = std::move(cb); }
    void set_on_uninstall(std::function<void(const std::filesystem::path&)> cb) { m_on_uninstall = std::move(cb); }

    [[nodiscard]] std::filesystem::path get_selected_path() const;

    // Toast feedback alert
    void show_toast(const std::string& msg) const;

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

    // Direct access for testing
    [[nodiscard]] FileToolbarWidget& toolbar() noexcept { return *m_toolbar; }
    [[nodiscard]] FileSidebarWidget& sidebar() noexcept { return *m_sidebar; }
    [[nodiscard]] FileIconGridView& grid_view() noexcept { return *m_grid_view; }
    [[nodiscard]] FileListView& list_view() noexcept { return *m_list_view; }
    [[nodiscard]] FileGalleryView& gallery_view() noexcept { return *m_gallery_view; }
    [[nodiscard]] FileInspectorWidget& inspector() noexcept { return *m_inspector; }
    [[nodiscard]] FileQuickLookModal& quick_look() noexcept { return *m_quick_look_modal; }
    [[nodiscard]] FileContextMenu& context_menu() noexcept { return *m_context_menu; }

private:
    // Child widgets
    txui::Ref<FileToolbarWidget>   m_toolbar;
    txui::Ref<FileSidebarWidget>   m_sidebar;
    txui::Ref<FileIconGridView>    m_grid_view;
    txui::Ref<FileListView>        m_list_view;
    txui::Ref<FileGalleryView>     m_gallery_view;
    txui::Ref<FileInspectorWidget> m_inspector;
    txui::Ref<FileQuickLookModal>  m_quick_look_modal;
    txui::Ref<FileContextMenu>     m_context_menu;

    // Column view components
    txui::Ref<txui::ScrollArea>    m_scroll_area;
    txui::Ref<txui::FlexLayout>    m_columns_layout;
    ColumnViewModel                m_column_model;

    // State
    std::filesystem::path          m_current_root;
    std::vector<FileItem>          m_current_items;
    ViewMode                       m_view_mode{ViewMode::IconGrid};
    SortCriteria                   m_sort_criteria{SortCriteria::Name};
    SortDirection                  m_sort_direction{SortDirection::Ascending};
    std::filesystem::path          m_selected_path;

    // History stack
    std::vector<std::filesystem::path> m_history;
    size_t                         m_history_idx{0};

    // Toast
    mutable std::string            m_toast_message;
    mutable std::chrono::steady_clock::time_point m_toast_time{};

    // Callbacks
    std::function<void(const std::filesystem::path&)> m_on_execute;
    std::function<void(const std::filesystem::path&)> m_on_trash;
    std::function<void(const std::filesystem::path&)> m_on_uninstall;

    void refresh_current_directory();
    void update_breadcrumbs();
    void rebuild_columns();
    void sync_active_view_items();

    // Core file operations
    void do_new_folder();
    void do_new_file();
    void do_copy();
    void do_cut();
    void do_paste();
    void do_trash();
    void do_rename();
};

} // namespace tinexus::files::ui
