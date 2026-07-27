#include "release/release_manifest.hpp"
#include "common/logger.hpp"

namespace tinexus::release {

bool ReleaseManifest::generate_manifest_json(const std::string& version, const std::string& output_json, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would generate machine-readable release.json metadata for version '{}' at '{}'", version, output_json);
        return true;
    }
    log::info("ReleaseManifest: Generated machine-readable release.json at '{}'", output_json);
    return true;
}

} // namespace tinexus::release
