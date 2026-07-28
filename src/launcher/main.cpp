#include "common/logger.hpp"
#include "common/version.hpp"
#include "launcher/theme_manager.hpp"
#include "launcher/launcher_controller.hpp"
#include "launcher/ipc_client.hpp"
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

namespace {
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    tinexus::log::info("launcher received signal {}, shutting down...", signal);
    g_running = false;
}
} // namespace

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-launcher");
    tinexus::log::info("Starting tinexus-launcher v{} - Command Palette UI", tinexus::VERSION_STRING);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    tinexus::launcher::ThemeManager::instance().set_dark_theme();
    tinexus::launcher::LauncherController::instance().show();

    tinexus::log::info("tinexus-launcher connected to wayland-0");
    tinexus::log::info("tinexus-launcher UI ready.");

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    tinexus::log::info("tinexus-launcher shutdown complete.");
    return 0;
}
