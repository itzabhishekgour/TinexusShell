#include "panel/panel_bar.hpp"
#include "common/logger.hpp"
#include <iostream>

using namespace tinexus;

int main() {
    log::info("tinexus-panel daemon starting...");
    log::info("tinexus-panel connected to wayland-0");

    auto& panel = panel::PanelBar::instance();
    panel.add_widget(std::make_unique<panel::LauncherWidget>());
    panel.add_widget(std::make_unique<panel::WorkspaceWidget>(1));
    panel.add_widget(std::make_unique<panel::ClockWidget>());
    panel.add_widget(std::make_unique<panel::CpuWidget>(8.4f));
    panel.add_widget(std::make_unique<panel::MemoryWidget>(3.8f));

    std::string content = panel.render_bar_content();
    log::info("tinexus-panel: Bound layer-shell Top surface (height=48px, exclusive_zone=48px)");
    log::info("tinexus-panel bar content: {}", content);

    return 0;
}
