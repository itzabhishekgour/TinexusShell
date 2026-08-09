#pragma once

#include <txui/widgets/Widget.hpp>
#include <string>

namespace tinexus::app_installer {

class InstallerWidget : public txui::Widget {
public:
    InstallerWidget();
    bool handle_event(const txui::Event& event) noexcept override;

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

private:
    std::string m_path_input;
    bool m_btn_hovered{false};
    bool m_btn_pressed{false};
    std::string m_status_msg;
    bool m_status_is_error{false};
    
    txui::Rect m_input_rect;
    txui::Rect m_btn_rect;
    
    void do_install();
};

} // namespace tinexus::app_installer
