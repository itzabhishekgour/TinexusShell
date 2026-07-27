#include "displayd/display_daemon.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main() {
    tinexus::log::set_component_name("tinexus-displayd");
    tinexus::log::info("Starting Tinexus Display Manager (tinexus-displayd)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Display Manager: Connected to Tinexus Platform IPC broker via SDK.");
    }

    tinexus::displayd::DisplayDaemon daemon;
    daemon.bootstrap();
    daemon.shutdown();

    sdk_client.disconnect();
    return 0;
}
