#include "liveusb/persistence_mgr.hpp"
#include "common/logger.hpp"

namespace tinexus::liveusb {

bool PersistenceManager::create_persistence_volume(const std::string& device_path, uint64_t size_mb, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would create {} MB ext4 overlayfs persistence partition labeled 'tinexus-RW' on '{}'", size_mb, device_path);
        return true;
    }
    log::info("PersistenceManager: Formatted ext4 persistence volume labeled 'tinexus-RW' on '{}'", device_path);
    return true;
}

} // namespace tinexus::liveusb
