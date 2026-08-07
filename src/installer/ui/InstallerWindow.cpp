#include "installer/InstallerWindow.hpp"
#include <txui/render/PixmanBackend.hpp>
#include <iostream>

namespace tinexus::installer {

InstallerWidget::InstallerWidget() 
{
    // Mock disks for UI placeholder
    m_available_disks = {"/dev/sda (512 GB NVMe)", "/dev/sdb (32 GB USB)"};
}

void InstallerWidget::next_step() {
    switch (m_state) {
        case InstallState::Welcome:
            m_state = InstallState::DiskSelection;
            break;
        case InstallState::DiskSelection:
            if (m_selected_disk_index >= 0) {
                m_state = InstallState::Confirmation;
            }
            break;
        case InstallState::Confirmation:
            m_state = InstallState::Installing;
            begin_installation();
            break;
        case InstallState::Installing:
        case InstallState::Finished:
            break;
    }
    mark_needs_paint(); // Trigger repaint
}

void InstallerWidget::prev_step() {
    switch (m_state) {
        case InstallState::DiskSelection:
            m_state = InstallState::Welcome;
            break;
        case InstallState::Confirmation:
            m_state = InstallState::DiskSelection;
            break;
        case InstallState::Welcome:
        case InstallState::Installing:
        case InstallState::Finished:
            break;
    }
    mark_needs_paint();
}

void InstallerWidget::begin_installation() {
    m_progress = 0.0;
    m_status_message = "Partitioning disk...";
    // In a real implementation, this would spawn a thread to call PartitionEngine
    // and SquashfsExtractor, reporting progress back to the UI.
}

void InstallerWidget::paint_override(txui::Painter& painter) const noexcept {
    // Clear background
    // In a full implementation, we'd use txui::PixmanBackend to draw text, buttons, and progress bars.
    // For now, this is a scaffolding class.
    
    switch (m_state) {
        case InstallState::Welcome:
            render_welcome_screen();
            break;
        case InstallState::DiskSelection:
            render_disk_selection();
            break;
        case InstallState::Confirmation:
            render_confirmation();
            break;
        case InstallState::Installing:
            render_installing();
            break;
        case InstallState::Finished:
            render_finished();
            break;
    }
}

void InstallerWidget::render_welcome_screen() const {}
void InstallerWidget::render_disk_selection() const {}
void InstallerWidget::render_confirmation() const {}
void InstallerWidget::render_installing() const {}
void InstallerWidget::render_finished() const {}

} // namespace tinexus::installer
