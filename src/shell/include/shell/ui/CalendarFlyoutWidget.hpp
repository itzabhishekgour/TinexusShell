#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>

namespace tinexus::shell {

class CalendarFlyoutWidget : public txui::Widget {
public:
    CalendarFlyoutWidget();

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

    void reset_to_current_month() noexcept { m_nav_offset = 0; mark_needs_paint(); }

private:
    int m_nav_offset{0};
    int m_hovered_day{-1};
    bool m_hover_prev{false};
    bool m_hover_next{false};
};

} // namespace tinexus::shell
