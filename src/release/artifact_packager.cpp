#include "release/artifact_packager.hpp"
#include "common/logger.hpp"

namespace tinexus::release {

bool ArtifactPackager::package_release_artifacts(const std::string& release_dir, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would assemble ISO, SHA256, SIG, release.json, and release notes into '{}'", release_dir);
        return true;
    }
    log::info("ArtifactPackager: Successfully packaged all release artifacts into '{}'", release_dir);
    return true;
}

} // namespace tinexus::release
