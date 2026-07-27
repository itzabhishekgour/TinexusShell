#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "pkg/package_manifest.hpp"
#include "pkg/crypto_verifier.hpp"
#include "pkg/dependency_solver.hpp"
#include "pkg/package_database.hpp"
#include "pkg/transaction_manager.hpp"
#include "pkg/package_manager.hpp"

void test_manifest_parser() {
    std::string text = "name: tinexus-test\nversion: 1.0.0\nsha256: abcdef123456\nfile: usr/bin/app\n";
    auto manifest = tinexus::pkg::ManifestParser::parse_string(text);
    assert(manifest.name == "tinexus-test");
    assert(manifest.version == "1.0.0");
    assert(manifest.is_valid());
    std::cout << "[PASS] test_manifest_parser\n";
}

void test_crypto_verifier() {
    std::string data = "sample_package_payload";
    auto hash = tinexus::pkg::CryptoVerifier::calculate_sha256(data);
    assert(!hash.empty());
    assert(tinexus::pkg::CryptoVerifier::verify_sha256(data, hash));
    assert(tinexus::pkg::CryptoVerifier::verify_signature(data, "valid_sig"));
    std::cout << "[PASS] test_crypto_verifier\n";
}

void test_dependency_solver_and_circular_rejection() {
    tinexus::pkg::PackageManifest a{"pkg-a", "1.0", "x86_64", {"pkg-b"}, {}, "hash_a", "sig_a"};
    tinexus::pkg::PackageManifest b{"pkg-b", "1.0", "x86_64", {}, {}, "hash_b", "sig_b"};
    tinexus::pkg::PackageManifest c1{"pkg-c1", "1.0", "x86_64", {"pkg-c2"}, {}, "hash_c1", "sig_c1"};
    tinexus::pkg::PackageManifest c2{"pkg-c2", "1.0", "x86_64", {"pkg-c1"}, {}, "hash_c2", "sig_c2"};

    std::vector<tinexus::pkg::PackageManifest> repo = {a, b, c1, c2};
    tinexus::pkg::DependencySolver solver;

    std::vector<std::string> order;
    assert(solver.resolve(repo, "pkg-a", order) == true);
    assert(order.size() == 2);
    assert(order[0] == "pkg-b");
    assert(order[1] == "pkg-a");

    std::vector<std::string> circ_order;
    assert(solver.resolve(repo, "pkg-c1", circ_order) == false); // Circular dependency rejected!
    std::cout << "[PASS] test_dependency_solver_and_circular_rejection\n";
}

void test_package_manager_install_remove() {
    tinexus::pkg::PackageManager pkg;
    tinexus::pkg::PackageManifest manifest{"app", "2.0.0", "x86_64", {}, {"usr/bin/app"}, "sha", "sig"};

    assert(pkg.install_package(manifest) == true);
    assert(pkg.is_installed("app") == true);
    assert(pkg.remove_package("app") == true);
    assert(pkg.is_installed("app") == false);
    std::cout << "[PASS] test_package_manager_install_remove\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_pkg");
    tinexus::log::info("Running Integration Test Suite for Tinexus Package Manager...");

    test_manifest_parser();
    test_crypto_verifier();
    test_dependency_solver_and_circular_rejection();
    test_package_manager_install_remove();

    tinexus::log::info("All Tinexus Package Manager integration tests passed 100%!");
    return 0;
}
