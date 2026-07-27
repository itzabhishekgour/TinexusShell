#include "iso/iso_builder.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("tinexus-iso");
    tinexus::log::info("Starting Tinexus ISO Builder Utility (tinexus-iso)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus ISO Builder: Connected to Tinexus Platform IPC broker via SDK.");
    }

    bool dry_run = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--dry-run") dry_run = true;
    }

    tinexus::iso::IsoBuilder builder;
    builder.build_iso("Tinexus-0.1.0-alpha.iso", dry_run);

    sdk_client.disconnect();
    return 0;
}
