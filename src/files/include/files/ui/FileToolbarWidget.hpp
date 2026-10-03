#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <files/file_model.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <functional>

namespace tinexus::files::ui {

enum class ViewMode : uint8_t {
    IconGrid = 0,
    List = 1,
    Column = 2,
    Gallery = 3
};

struct BreadcrumbItem {
    std::string name;
    std::filesystem::path path;
    txui::Rect rect{};
    bool hovered{false};
};

class FileToolbarWidget : public txui::Widget {
public:
    std::function<void()> on_back;
    std::function<void()> on_forward;
    std::function<void(ViewMode)> on_view_mode_changed;
    std::function<void()> on_new_item_clicked;
    std::function<void(const std::filesystem::path&)> on_breadcrumb_clicked;
    std::function<void()> on_sort_clicked;
    std::function<void(const std::string&)> on_search_changed;

    FileToolbarWidget();
    ~FileToolbarWidget() override = default;

    void set_history_state(bool can_back, bool can_forward) noexcept;
    void set_view_mode(ViewMode mode) noexcept;
    void set_sort(SortCriteria criteria, SortDirection direction) noexcept;
    void set_breadcrumbs(const std::vector<std::pair<std::string, std::filesystem::path>>& crumbs);
    void set_search_query(const std::string& query);
    [[nodiscard]] const std::string& search_query() const noexcept { return m_search_query; }
    [[nodiscard]] bool is_search_focused() const noexcept { return m_search_focused; }
    void clear_search();

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    bool m_can_back{false};
    bool m_can_forward{false};
    ViewMode m_view_mode{ViewMode::IconGrid};
    SortCriteria m_sort_criteria{SortCriteria::Name};
    SortDirection m_sort_direction{SortDirection::Ascending};
    std::string m_search_query;
    bool m_search_focused{false};

    std::vector<BreadcrumbItem> m_breadcrumbs;

    // Hit rects
    txui::Rect m_back_rect{};
    txui::Rect m_fwd_rect{};
    txui::Rect m_view_grid_rect{};
    txui::Rect m_view_list_rect{};
    txui::Rect m_view_col_rect{};
    txui::Rect m_view_gal_rect{};
    txui::Rect m_new_btn_rect{};
    txui::Rect m_sort_btn_rect{};
    txui::Rect m_search_rect{};
    txui::Rect m_search_clear_rect{};

    // Hover states
    bool m_back_hovered{false};
    bool m_fwd_hovered{false};
    bool m_view_grid_hovered{false};
    bool m_view_list_hovered{false};
    bool m_view_col_hovered{false};
    bool m_view_gal_hovered{false};
    bool m_new_btn_hovered{false};
    bool m_sort_hovered{false};
    bool m_search_hovered{false};
};

} // namespace tinexus::files::ui
