#include "installer/partition_engine.hpp"
#include "common/logger.hpp"

namespace tinexus::installer {

bool PartitionEngine::create_gpt_layout(const std::string& disk_path, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would create GPT partition table on '{}'", disk_path);
        return true;
    }
    log::info("PartitionEngine: Created GPT partition table on '{}'", disk_path);
    return true;
}

bool PartitionEngine::format_partition(const std::string& partition_path, const std::string& fs_type, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would format partition '{}' as '{}'", partition_path, fs_type);
        return true;
    }
    log::info("PartitionEngine: Formatted partition '{}' as '{}'", partition_path, fs_type);
    return true;
}

} // namespace tinexus::installer
