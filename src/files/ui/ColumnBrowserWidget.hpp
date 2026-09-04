#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/layout/ScrollArea.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/widgets/Label.hpp>
#include <txui/widgets/Icon.hpp>
#include <functional>
#include <vector>
#include <filesystem>
#include "files/column_view_model.hpp"

namespace tinexus::files::ui {

struct SidebarBookmark {
    std::string name;
    std::filesystem::path path;
    txui::IconType icon_type;
    txui::Rect rect{};
    bool hovered{false};
};

struct BreadcrumbPill {
    std::string name;
    std::filesystem::path path;
    txui::Rect rect{};
    bool hovered{false};
};

class ColumnBrowserWidget : public txui::Widget {
private:
    txui::Ref<txui::ScrollArea> m_scroll_area;
    txui::Ref<txui::FlexLayout> m_columns_layout;

    ColumnViewModel m_model;
    std::filesystem::path m_current_root;

    // History stack for Back/Forward
    std::vector<std::filesystem::path> m_history;
    size_t m_history_idx{0};

    // Bookmarks in sidebar
    std::vector<SidebarBookmark> m_bookmarks;

    // Breadcrumbs
    std::vector<BreadcrumbPill> m_breadcrumbs;

    // Toolbar buttons rects
    txui::Rect m_back_btn_rect{};
    txui::Rect m_fwd_btn_rect{};
    txui::Rect m_up_btn_rect{};
    bool m_back_hovered{false};
    bool m_fwd_hovered{false};
    bool m_up_hovered{false};

    // Inspector Action Buttons
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

    void on_item_selected(size_t col_index, size_t item_index);
    void on_item_double_clicked(size_t col_index, size_t item_index);
    void update_inspector_state(size_t col_index, size_t item_index);
    void rebuild_breadcrumbs();
    void rebuild_columns();

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

public:
    ColumnBrowserWidget();
    ~ColumnBrowserWidget() override = default;

    void navigate_to(const std::filesystem::path& path, bool record_history = true);
    void go_back();
    void go_forward();
    void go_up();

    void set_on_execute(std::function<void(const std::filesystem::path&)> callback) { m_on_execute = std::move(callback); }
    void set_on_trash(std::function<void(const std::filesystem::path&)> callback) { m_on_trash = std::move(callback); }
    void set_on_uninstall(std::function<void(const std::filesystem::path&)> callback) { m_on_uninstall = std::move(callback); }

    [[nodiscard]] std::filesystem::path get_selected_path() const;

    bool handle_event(const txui::Event& event) noexcept override;
};

} // namespace tinexus::files::ui
