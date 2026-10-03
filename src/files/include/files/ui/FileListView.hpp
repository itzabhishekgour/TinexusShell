#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <files/file_model.hpp>
#include <vector>
#include <string>
#include <functional>
#include <chrono>

namespace tinexus::files::ui {

struct ListRowHit {
    size_t index{0};
    txui::Rect row_rect{};
    std::filesystem::path path;
};

class FileListView : public txui::Widget {
public:
    std::function<void(int32_t, const FileItem&)> on_item_selected;
    std::function<void(int32_t, const FileItem&)> on_item_double_clicked;
    std::function<void(double x, double y, const std::filesystem::path&)> on_context_menu;
    std::function<void(SortCriteria)> on_sort_changed;

    FileListView();
    ~FileListView() override = default;

    void set_items(const std::vector<FileItem>& items);
    void set_sort(SortCriteria criteria, SortDirection direction) noexcept;
    void set_selected_index(int32_t idx) noexcept;
    void select_path(const std::filesystem::path& path);
    [[nodiscard]] int32_t selected_index() const noexcept { return m_selected_idx; }

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    std::vector<FileItem> m_items;
    int32_t m_selected_idx{-1};
    SortCriteria m_sort_criteria{SortCriteria::Name};
    SortDirection m_sort_direction{SortDirection::Ascending};
    double m_scroll_y{0.0};
    double m_max_scroll_y{0.0};

    // Header rects
    txui::Rect m_hdr_name_rect{};
    txui::Rect m_hdr_date_rect{};
    txui::Rect m_hdr_size_rect{};
    txui::Rect m_hdr_kind_rect{};

    mutable std::vector<ListRowHit> m_rows;

    std::chrono::steady_clock::time_point m_last_click_time{};
    int32_t m_last_clicked_idx{-1};
};

} // namespace tinexus::files::ui
