#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <filesystem>
#include <vector>
#include <string>
#include <functional>

namespace tinexus::files::ui {

class FileQuickLookModal : public txui::Widget {
public:
    std::function<void()> on_close;

    FileQuickLookModal();
    ~FileQuickLookModal() override = default;

    void open_file(const std::filesystem::path& path);
    void close_modal() noexcept;
    [[nodiscard]] bool is_open() const noexcept { return m_is_open; }

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    bool m_is_open{false};
    std::filesystem::path m_path;
    std::vector<std::string> m_lines;

    txui::Rect m_card_rect{};
    txui::Rect m_close_rect{};
    bool m_close_hovered{false};
};

} // namespace tinexus::files::ui
