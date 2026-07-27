#include "pkg/package_manager.hpp"
#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("tinexus-pkg");
    tinexus::log::info("Starting Tinexus Package Manager (tinexus-pkg)...");

    tinexus::Client sdk_client;
    if (sdk_client.connect().is_ok()) {
        tinexus::log::info("Tinexus Package Manager: Connected to Tinexus Platform IPC broker via SDK.");
    }

    tinexus::pkg::PackageManager pkg_mgr;
    tinexus::pkg::PackageManifest manifest;
    manifest.name = (argc > 1) ? argv[1] : "tinexus-terminal";
    manifest.version = "1.0.0";
    manifest.sha256_checksum = "mock_sha256_pass";

    pkg_mgr.install_package(manifest);

    sdk_client.disconnect();
    return 0;
}
