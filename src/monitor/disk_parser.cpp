#include "monitor/disk_parser.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>

namespace tinexus::monitor {

DiskMetrics DiskParser::parse_diskstats() {
    DiskMetrics disk;
    std::ifstream file("/proc/diskstats");
    if (!file.is_open()) return disk;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        uint32_t major, minor;
        std::string name;
        uint64_t reads_completed, reads_merged, sectors_read, time_reading;
        uint64_t writes_completed, writes_merged, sectors_written, time_writing;

        if (ss >> major >> minor >> name >> reads_completed >> reads_merged >> sectors_read >> time_reading >> writes_completed >> writes_merged >> sectors_written >> time_writing) {
            if (name == "sda" || name == "nvme0n1") {
                disk.read_bytes_sec += sectors_read * 512;
                disk.write_bytes_sec += sectors_written * 512;
                disk.iops += static_cast<uint32_t>(reads_completed + writes_completed);
            }
        }
    }
    return disk;
}

} // namespace tinexus::monitor
