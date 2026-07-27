#ifndef TINEXUS_INSTALLER_PARTITION_ENGINE_HPP
#define TINEXUS_INSTALLER_PARTITION_ENGINE_HPP

#include <string>

namespace tinexus::installer {

class PartitionEngine {
public:
    PartitionEngine() = default;
    ~PartitionEngine() = default;

    bool create_gpt_layout(const std::string& disk_path, bool dry_run = false);
    bool format_partition(const std::string& partition_path, const std::string& fs_type, bool dry_run = false);
};

} // namespace tinexus::installer

#endif // TINEXUS_INSTALLER_PARTITION_ENGINE_HPP
