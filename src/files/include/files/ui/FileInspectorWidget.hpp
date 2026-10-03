#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <filesystem>
#include <string>
#include <functional>

namespace tinexus::files::ui {

class FileInspectorWidget : public txui::Widget {
public:
    std::function<void(const std::filesystem::path&)> on_open_requested;
    std::function<void(const std::filesystem::path&)> on_trash_requested;

    FileInspectorWidget();
    ~FileInspectorWidget() override = default;

    void inspect_path(const std::filesystem::path& path);
    [[nodiscard]] const std::filesystem::path& inspected_path() const noexcept { return m_path; }

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    std::filesystem::path m_path;
    bool m_is_dir{false};
    bool m_is_image{false};
    std::string m_disp_name{"No selection"};
    std::string m_kind_str;
    std::string m_size_str;
    std::string m_modified_str;

    txui::Rect m_preview_box{};
    txui::Rect m_open_btn_rect{};
    txui::Rect m_trash_btn_rect{};
    bool m_open_hovered{false};
    bool m_trash_hovered{false};
};

} // namespace tinexus::files::ui
