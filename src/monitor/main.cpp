#include "monitor/resource_monitor.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main() {
    tinexus::log::set_component_name("monitor");
    tinexus::log::info("Starting Tinexus System Monitor (tinexus-monitor)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Monitor: Connected to Tinexus Platform IPC broker via SDK.");
    }

    auto snap = tinexus::monitor::ResourceMonitor::instance().collect_snapshot();
    tinexus::log::info("Tinexus Monitor running actively. Initialized system inspection.");

    sdk_client.disconnect();
    return 0;
}
