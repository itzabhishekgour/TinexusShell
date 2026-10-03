#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/Slider.hpp>
#include <txui/widgets/Button.hpp>

namespace tinexus::shell {

class VolumeFlyoutWidget : public txui::Widget {
public:
    VolumeFlyoutWidget();
    ~VolumeFlyoutWidget() override = default;

    void refresh_state();

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    txui::Ref<txui::Slider> m_slider;
    txui::Ref<txui::Button> m_mute_btn;
    txui::Ref<txui::Button> m_test_btn;

    std::string m_control_name{"Master"};
    int         m_current_volume{75};
    bool        m_is_muted{false};
};

} // namespace tinexus::shell
