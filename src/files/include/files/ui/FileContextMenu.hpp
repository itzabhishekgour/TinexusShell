#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <functional>

namespace tinexus::files::ui {

enum class FileContextAction : uint8_t {
    Open,
    QuickLook,
    Rename,
    Copy,
    Cut,
    Paste,
    Trash,
    GetInfo,
    NewFolder,
    NewFile,
    Refresh,
    ClearTag
};

struct ContextOption {
    FileContextAction action;
    std::string label;
    bool enabled{true};
    txui::Rect rect{};
    bool hovered{false};
};

class FileContextMenu : public txui::Widget {
public:
    std::function<void(FileContextAction, const std::filesystem::path&)> on_action_selected;
    std::function<void(const std::string& tag, const std::filesystem::path&)> on_tag_selected;
    std::function<void()> on_dismiss;

    FileContextMenu();
    ~FileContextMenu() override = default;

    void show_at(double x, double y, const std::filesystem::path& path, bool is_background = false);
    void dismiss() noexcept;
    [[nodiscard]] bool is_open() const noexcept { return m_is_open; }

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    bool m_is_open{false};
    bool m_is_background{false};
    txui::Point m_pos{};
    std::filesystem::path m_target_path;

    txui::Rect m_menu_rect{};
    std::vector<ContextOption> m_options;
    std::vector<std::pair<std::string, txui::Rect>> m_tag_dots;
    txui::Rect m_clear_tag_rect{};
    bool m_clear_tag_hovered{false};
};

} // namespace tinexus::files::ui
