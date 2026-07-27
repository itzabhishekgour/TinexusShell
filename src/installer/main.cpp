#include "installer/install_controller.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("tinexus-installer");
    tinexus::log::info("Starting Tinexus Graphical OS Installer (tinexus-installer)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Installer: Connected to Tinexus Platform IPC broker via SDK.");
    }

    bool dry_run = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--dry-run") dry_run = true;
    }

    tinexus::installer::DiskInspector inspector;
    auto disks = inspector.discover_disks();
    if (!disks.empty()) {
        tinexus::installer::InstallController controller;
        controller.run_installation(disks[0], dry_run);
    }

    sdk_client.disconnect();
    return 0;
}
