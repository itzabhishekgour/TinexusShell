#include "common/logger.hpp"
#include "common/version.hpp"
#include "comp/server/server.hpp"
#include <csignal>

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
    tinexus::log::set_component_name("tinexus-comp");
    tinexus::log::info("Starting tinexus-comp v{} - Wayland Vulkan Compositor", tinexus::VERSION_STRING);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGHUP, signal_handler);

    tinexus::comp::TinexusServer server;
    g_server = &server;

    if (!server.initialize()) {
        tinexus::log::error("Failed to initialize Tinexus Compositor Server");
        return 1;
    }

    if (!server.run()) {
        tinexus::log::error("Tinexus Compositor run failed");
        return 1;
    }
    return 0;
}
