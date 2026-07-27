#include "clipboard/clipboard_manager.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main() {
    tinexus::log::set_component_name("clipboard");
    tinexus::log::info("Starting Tinexus Clipboard History Manager (tinexus-clip)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Clipboard: Connected to Tinexus Platform IPC broker via SDK.");
    }

    tinexus::log::info("Tinexus Clipboard Manager running actively.");
    sdk_client.disconnect();
    return 0;
}
