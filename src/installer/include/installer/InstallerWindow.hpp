#ifndef TINEXUS_INSTALLER_WINDOW_HPP
#define TINEXUS_INSTALLER_WINDOW_HPP

#include <txui/window/Window.hpp>
#include <txui/widgets/Widget.hpp>
#include <txui/animation/CursorAnimator.hpp>
#include <memory>
#include <string>
#include <vector>

namespace tinexus::installer {

enum class InstallState {
    Welcome,
    DiskSelection,
    Confirmation,
    Installing,
    Finished
};

class InstallerWidget : public txui::Widget {
public:
    InstallerWidget();
    ~InstallerWidget() override = default;

    // Transition to the next logical step in the installation wizard
    void next_step();
    // Transition to the previous step (if applicable)
    void prev_step();

    // Start the actual installation process (triggers backend)
    void begin_installation();

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override { return txui::Size(constraints.max_width, constraints.max_height); }
    void paint_override(txui::Painter& painter) const noexcept override;

private:
    void render_welcome_screen() const;
    void render_disk_selection() const;
    void render_confirmation() const;
    void render_installing() const;
    void render_finished() const;

    InstallState m_state{InstallState::Welcome};
    std::vector<std::string> m_available_disks;
    int m_selected_disk_index{-1};
    
    double m_progress{0.0};
    std::string m_status_message;
    
    txui::CursorAnimator m_cursor_animator;
};

} // namespace tinexus::installer

#endif // TINEXUS_INSTALLER_WINDOW_HPP
