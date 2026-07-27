#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "release/checksum_generator.hpp"
#include "release/signature_engine.hpp"
#include "release/release_notes_gen.hpp"
#include "release/release_manifest.hpp"
#include "release/artifact_packager.hpp"
#include "release/git_release.hpp"
#include "release/release_pipeline.hpp"

void test_checksum_and_signature_engine() {
    assert(tinexus::release::ChecksumGenerator::generate_sha256("Tinexus.iso", "Tinexus.iso.sha256", true));
    assert(tinexus::release::SignatureEngine::sign_file("Tinexus.iso", "Tinexus.iso.sig", true));
    assert(tinexus::release::SignatureEngine::verify_signature("Tinexus.iso", "Tinexus.iso.sig", true));
    std::cout << "[PASS] test_checksum_and_signature_engine\n";
}

void test_release_notes_and_manifest() {
    assert(tinexus::release::ReleaseNotesGen::generate_release_notes("v0.1.0-alpha", "RELEASE_NOTES.md", true));
    assert(tinexus::release::ReleaseManifest::generate_manifest_json("v0.1.0-alpha", "release.json", true));
    std::cout << "[PASS] test_release_notes_and_manifest\n";
}

void test_artifact_packager_and_git_release() {
    assert(tinexus::release::ArtifactPackager::package_release_artifacts("release/", true));
    assert(tinexus::release::GitRelease::validate_git_tag_readiness("v0.1.0-alpha"));
    std::cout << "[PASS] test_artifact_packager_and_git_release\n";
}

void test_master_release_pipeline() {
    tinexus::release::ReleasePipeline pipeline;
    assert(pipeline.run_release_pipeline("v0.1.0-alpha", true));
    std::cout << "[PASS] test_master_release_pipeline\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_release");
    tinexus::log::info("Running Integration Test Suite for Tinexus Master Release Engineering...");

    test_checksum_and_signature_engine();
    test_release_notes_and_manifest();
    test_artifact_packager_and_git_release();
    test_master_release_pipeline();

    tinexus::log::info("All Tinexus Release Engineering integration tests passed 100%!");
    return 0;
}
