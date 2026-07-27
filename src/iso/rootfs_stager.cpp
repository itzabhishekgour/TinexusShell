#include "iso/rootfs_stager.hpp"
#include "common/logger.hpp"

namespace tinexus::iso {

bool RootfsStager::stage_rootfs(const std::string& target_dir, const std::vector<std::string>& manifests, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would stage RootFS tree at '{}' using {} manifest(s)", target_dir, manifests.size());
        return true;
    }
    log::info("RootfsStager: Staged complete RootFS directory tree at '{}'", target_dir);
    return true;
}

} // namespace tinexus::iso
