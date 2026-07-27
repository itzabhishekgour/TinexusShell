#include "release/release_pipeline.hpp"
#include "release/checksum_generator.hpp"
#include "release/signature_engine.hpp"
#include "release/release_notes_gen.hpp"
#include "release/release_manifest.hpp"
#include "release/artifact_packager.hpp"
#include "release/git_release.hpp"
#include "common/logger.hpp"

namespace tinexus::release {

bool ReleasePipeline::run_release_pipeline(const std::string& version, bool dry_run) {
    log::info("ReleasePipeline: Executing master Tinexus Release Engineering Pipeline for version '{}' (Dry-Run: {})...", version, dry_run ? "TRUE" : "FALSE");

    std::string iso_path = "release/Tinexus-0.1.0-alpha.iso";
    std::string sha_path = "release/Tinexus-0.1.0-alpha.iso.sha256";
    std::string sig_path = "release/Tinexus-0.1.0-alpha.iso.sig";
    std::string notes_path = "release/RELEASE_NOTES_v0.1.0-alpha.md";
    std::string manifest_path = "release/release.json";

    if (!ChecksumGenerator::generate_sha256(iso_path, sha_path, dry_run)) return false;
    if (!SignatureEngine::sign_file(iso_path, sig_path, dry_run)) return false;
    if (!SignatureEngine::verify_signature(iso_path, sig_path, dry_run)) return false;
    if (!ReleaseNotesGen::generate_release_notes(version, notes_path, dry_run)) return false;
    if (!ReleaseManifest::generate_manifest_json(version, manifest_path, dry_run)) return false;
    if (!ArtifactPackager::package_release_artifacts("release/", dry_run)) return false;
    if (!GitRelease::validate_git_tag_readiness(version)) return false;

    log::info("ReleasePipeline: Tinexus Platform v0.1.0-alpha release pipeline READY FOR FIRST PUBLIC ALPHA!");
    return true;
}

} // namespace tinexus::release
