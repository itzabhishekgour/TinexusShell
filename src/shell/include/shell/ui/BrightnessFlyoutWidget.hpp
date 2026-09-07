#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/Slider.hpp>
#include <txui/widgets/Button.hpp>

namespace tinexus::shell {

class BrightnessFlyoutWidget : public txui::Widget {
public:
    BrightnessFlyoutWidget();
    ~BrightnessFlyoutWidget() override = default;

    void refresh_state();

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    txui::Ref<txui::Slider> m_slider;
    txui::Ref<txui::Button> m_theme_btn;

    std::string m_device_name{"Display"};
    int         m_current_brightness{80};
    bool        m_dark_theme{true};
};

} // namespace tinexus::shell
