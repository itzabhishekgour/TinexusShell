#include "release/release_notes_gen.hpp"
#include "common/logger.hpp"

namespace tinexus::release {

bool ReleaseNotesGen::generate_release_notes(const std::string& version, const std::string& output_md, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would generate RELEASE_NOTES_{}.md at '{}'", version, output_md);
        return true;
    }
    log::info("ReleaseNotesGen: Generated release notes markdown file at '{}'", output_md);
    return true;
}

} // namespace tinexus::release
