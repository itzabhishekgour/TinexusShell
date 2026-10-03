#include "monitor/disk_parser.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>
#include <sys/statvfs.h>
#include <cctype>
#include <algorithm>

namespace tinexus::monitor {

namespace {

bool is_primary_disk(const std::string& name) {
    if (name.empty()) return false;
    // SCSI / SATA / USB disk: sda, sdb, ...
    if (name.rfind("sd", 0) == 0 && name.length() == 3 && std::isalpha(name[2])) return true;
    // VirtIO disk: vda, vdb, ...
    if (name.rfind("vd", 0) == 0 && name.length() == 3 && std::isalpha(name[2])) return true;
    // Xen disk: xvda, xvdb, ...
    if (name.rfind("xvd", 0) == 0 && name.length() == 4 && std::isalpha(name[3])) return true;
    // NVMe whole disk: nvme0n1, nvme1n1 (no 'p' partition suffix)
    if (name.rfind("nvme", 0) == 0 && name.find('p') == std::string::npos && name.find('n') != std::string::npos) return true;
    // MMC whole disk: mmcblk0, mmcblk1 (no 'p' partition suffix)
    if (name.rfind("mmcblk", 0) == 0 && name.find('p') == std::string::npos) return true;
    return false;
}

} // namespace

DiskMetrics DiskParser::parse_diskstats() {
    DiskMetrics disk;
    auto now = std::chrono::steady_clock::now();

    // 1. Filesystem capacity via statvfs
    struct statvfs sv;
    if (statvfs("/", &sv) == 0) {
        disk.root_total_bytes = sv.f_blocks * sv.f_frsize;
        disk.root_free_bytes = sv.f_bfree * sv.f_frsize;
        disk.root_used_bytes = (disk.root_total_bytes > disk.root_free_bytes)
                                   ? (disk.root_total_bytes - disk.root_free_bytes)
                                   : 0;
    }

    // 2. Real-time I/O throughput via delta calculations
    std::ifstream file("/proc/diskstats");
    if (!file.is_open()) return disk;

    uint64_t total_sectors_read = 0;
    uint64_t total_sectors_written = 0;
    uint64_t total_io_ops = 0;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        uint32_t major, minor;
        std::string name;
        uint64_t reads_completed, reads_merged, sectors_read, time_reading;
        uint64_t writes_completed, writes_merged, sectors_written, time_writing;

        if (ss >> major >> minor >> name >> reads_completed >> reads_merged
               >> sectors_read >> time_reading >> writes_completed
               >> writes_merged >> sectors_written >> time_writing) {
            if (is_primary_disk(name)) {
                total_sectors_read += sectors_read;
                total_sectors_written += sectors_written;
                total_io_ops += (reads_completed + writes_completed);
            }
        }
    }

    if (m_has_prev) {
        double elapsed_sec = std::chrono::duration<double>(now - m_prev_time).count();
        if (elapsed_sec > 0.05) {
            uint64_t d_read = (total_sectors_read >= m_prev_sectors_read)
                                  ? (total_sectors_read - m_prev_sectors_read)
                                  : 0;
            uint64_t d_write = (total_sectors_written >= m_prev_sectors_written)
                                   ? (total_sectors_written - m_prev_sectors_written)
                                   : 0;
            uint64_t d_ops = (total_io_ops >= m_prev_io_count)
                                 ? (total_io_ops - m_prev_io_count)
                                 : 0;

            disk.read_bytes_sec = static_cast<uint64_t>((static_cast<double>(d_read) * 512.0) / elapsed_sec);
            disk.write_bytes_sec = static_cast<uint64_t>((static_cast<double>(d_write) * 512.0) / elapsed_sec);
            disk.iops = static_cast<uint32_t>(static_cast<double>(d_ops) / elapsed_sec);
        }
    }

    m_prev_sectors_read = total_sectors_read;
    m_prev_sectors_written = total_sectors_written;
    m_prev_io_count = total_io_ops;
    m_prev_time = now;
    m_has_prev = true;

    return disk;
}

} // namespace tinexus::monitor
