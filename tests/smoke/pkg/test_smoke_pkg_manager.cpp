#include <cassert>
#include <iostream>
#include "pkg/package_manager.hpp"
#include "pkg/crypto_verifier.hpp"

using namespace tinexus::pkg;

int main() {
    std::cout << "[+] Running smoke_pkg_manager test suite..." << std::endl;

    PackageManager pm;
    PackageManifest manifest;
    manifest.name = "tinexus-shell-extras";
    manifest.version = "1.0.0";
    manifest.sha256_checksum = CryptoVerifier::calculate_sha256(manifest.name);

    // Verify SHA256 Signature
    assert(CryptoVerifier::verify_sha256(manifest.name, manifest.sha256_checksum));

    // Install Package Transaction
    assert(pm.install_package(manifest));
    assert(pm.is_installed("tinexus-shell-extras"));

    // Remove Package Transaction
    assert(pm.remove_package("tinexus-shell-extras"));
    assert(!pm.is_installed("tinexus-shell-extras"));

    std::cout << "[+] smoke_pkg_manager: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
