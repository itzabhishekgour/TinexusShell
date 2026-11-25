#include "common/logger.hpp"
#include "common/RuntimePaths.hpp"
#include "common/version.hpp"
#include "comp/server/server.hpp"
#include <csignal>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
tinexus::comp::TinexusServer* g_server{nullptr};

void signal_handler(int signal) {
    if (g_server) {
        tinexus::log::info("Received signal {}, initiating compositor shutdown...", signal);
        g_server->stop();
    }
}
} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    tinexus::log::set_component_name("tinexus-comp");

    int boot_tier = 1;
    for (const std::string& p : {
             std::string("/run/tinexus/fallback_tier"),
             tinexus::common::RuntimePaths::get_runtime_dir() + "/fallback_tier"}) {
        std::ifstream tf(p);
        if (tf.is_open()) {
            int val = 1;
            if (tf >> val && val >= 1 && val <= 4) {
                boot_tier = val;
            }
            break;
        }
    }
    tinexus::log::info("Starting tinexus-comp v{} - Wayland Compositor (tier={})",
                       tinexus::VERSION_STRING, boot_tier);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);
    std::signal(SIGPIPE, SIG_IGN);

    tinexus::comp::TinexusServer server;
    g_server = &server;

    if (!server.initialize()) {
        std::error_code ec;
        std::filesystem::remove("/run/tinexus/fallback_tier", ec);
        std::filesystem::remove(tinexus::common::RuntimePaths::get_runtime_dir() + "/fallback_tier", ec);
        tinexus::log::error("Failed to initialize Tinexus Compositor Server");
        return 1;
    }

    if (!server.run()) {
        tinexus::log::error("Tinexus Compositor run failed");
        return 1;
    }
    return 0;
}
