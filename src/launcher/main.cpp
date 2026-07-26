#include "common/logger.hpp"
#include "common/version.hpp"
#include "launcher/theme_manager.hpp"
#include "launcher/launcher_controller.hpp"
#include "launcher/ipc_client.hpp"

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-launcher");
    tinexus::log::info("Starting tinexus-launcher v{} - Command Palette UI", tinexus::VERSION_STRING);

    tinexus::launcher::ThemeManager::instance().set_dark_theme();
    tinexus::launcher::LauncherController::instance().show();

    tinexus::log::info("tinexus-launcher UI ready.");
    return 0;
}
