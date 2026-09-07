#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <functional>
#include <string>
#include <vector>

namespace tinexus::shell {

enum class LogoMenuAction {
    AboutTinexus,
    SystemSettings,
    AppInstaller,
    SystemMonitor,
    Sleep,
    Restart,
    ShutDown,
    LockScreen
};

class LogoMenuWidget : public txui::Widget {
public:
    std::function<void(LogoMenuAction action)> on_action_selected;

    LogoMenuWidget();

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    int m_hovered_index{-1};
};

} // namespace tinexus::shell
