#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <functional>

namespace tinexus::shell {

class AuraNotchWidget : public txui::Widget {
public:
    std::function<void()> on_center_clicked;
    std::function<void()> on_date_clicked;

    AuraNotchWidget();

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

    void set_calendar_open(bool open) noexcept {
        if (m_calendar_open != open) {
            m_calendar_open = open;
            mark_needs_paint();
        }
    }

private:
    bool m_hover_center{false};
    bool m_hover_date{false};
    bool m_calendar_open{false};
};

} // namespace tinexus::shell
