#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <txui/widgets/Icon.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <functional>

namespace tinexus::files::ui {

struct SidebarEntry {
    std::string name;
    std::filesystem::path path;
    txui::IconType icon_type;
    txui::Rect rect{};
    bool hovered{false};
    bool is_section_header{false};
};

class FileSidebarWidget : public txui::Widget {
public:
    std::function<void(const std::filesystem::path&)> on_location_selected;

    FileSidebarWidget();
    ~FileSidebarWidget() override = default;

    void refresh_locations();
    void set_active_path(const std::filesystem::path& path) noexcept;

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    std::vector<SidebarEntry> m_entries;
    std::filesystem::path m_active_path;
};

} // namespace tinexus::files::ui
