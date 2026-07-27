#include "release/release_pipeline.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("tinexus-release");
    tinexus::log::info("Starting Tinexus Master Release Engineering Utility (tinexus-release)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Release Utility: Connected to Tinexus Platform IPC broker via SDK.");
    }

    bool dry_run = true;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--dry-run") dry_run = true;
    }

    tinexus::release::ReleasePipeline pipeline;
    pipeline.run_release_pipeline("v0.1.0-alpha", dry_run);

    sdk_client.disconnect();
    return 0;
}
