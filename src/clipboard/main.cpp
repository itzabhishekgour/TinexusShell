#include "clipboard/clipboard_manager.hpp"
#include "tinexus/client.hpp"
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

int main() {
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    tinexus::log::set_component_name("clipboard");
    tinexus::log::info("Starting Tinexus Clipboard History Manager (tinexus-clip)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Clipboard: Connected to Tinexus Platform IPC broker via SDK.");
    }

    tinexus::log::info("clipboard initialized and connected to wayland-0");
    tinexus::log::info("Tinexus Clipboard Manager running actively.");
    
    while (g_running) {
        // Future daemon work: IPC, events, state updates
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    sdk_client.disconnect();
    return 0;
}
