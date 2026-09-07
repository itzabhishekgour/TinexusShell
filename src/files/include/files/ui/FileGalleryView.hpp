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

struct GalleryStripHit {
    size_t index{0};
    txui::Rect cell_rect{};
    std::filesystem::path path;
};

class FileGalleryView : public txui::Widget {
public:
    std::function<void(int32_t, const FileItem&)> on_item_selected;
    std::function<void(int32_t, const FileItem&)> on_item_double_clicked;
    std::function<void(double x, double y, const std::filesystem::path&)> on_context_menu;

    FileGalleryView();
    ~FileGalleryView() override = default;

    void set_items(const std::vector<FileItem>& items);
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

    mutable std::vector<GalleryStripHit> m_strip_hits;

    std::chrono::steady_clock::time_point m_last_click_time{};
    int32_t m_last_clicked_idx{-1};
};

} // namespace tinexus::files::ui
