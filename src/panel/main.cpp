#include "panel/panel_bar.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>

namespace {
std::atomic<bool> g_running{true};
void signal_handler(int) {
    g_running = false;
}
} // namespace
using namespace tinexus;

int main() {
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

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

    while (g_running) {
        // Future daemon work: IPC, events, state updates
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    return 0;
}
