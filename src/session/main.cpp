#include "session/session_manager.hpp"
#include "session/env_bootstrap.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <cstring>

void print_usage() {
    std::cout << "Usage: tinexus-session [OPTIONS]\n"
              << "Options:\n"
              << "  --dry-run       Validate environment bootstrap without launching services\n"
              << "  --print-env     Print complete POSIX environment map\n"
              << "  --validate      Validate session dependencies and permissions\n"
              << "  --version       Display session manager version\n"
              << "  --help          Display this help message\n";
}

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("session");

    bool dry_run = false;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--dry-run") == 0) {
            dry_run = true;
        } else if (std::strcmp(argv[i], "--print-env") == 0) {
            tinexus::session::EnvironmentBootstrapper::instance().print_environment();
            return 0;
        } else if (std::strcmp(argv[i], "--version") == 0) {
            std::cout << "tinexus-session v1.0.0 (ABI v1.0 Locked)\n";
            return 0;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
    }

    tinexus::log::info("Starting Tinexus Session Manager (tinexus-session)...");
    if (!tinexus::session::SessionManager::instance().start_session(dry_run)) {
        tinexus::log::error("Failed to start Tinexus session!");
        return 1;
    }

    if (dry_run) {
        tinexus::log::info("Dry run completed successfully!");
        return 0;
    }

    tinexus::log::info("Tinexus Desktop Session running actively.");
    return 0;
}
