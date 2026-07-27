#ifndef TINEXUS_MONITOR_METRICS_SNAPSHOT_HPP
#define TINEXUS_MONITOR_METRICS_SNAPSHOT_HPP

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace tinexus::monitor {

struct CpuCoreMetrics {
    uint32_t core_id{0};
    float usage_percent{0.0f};
};

struct MemoryMetrics {
    uint64_t total_ram_bytes{0};
    uint64_t available_ram_bytes{0};
    uint64_t total_swap_bytes{0};
    uint64_t free_swap_bytes{0};
};

struct DiskMetrics {
    uint64_t read_bytes_sec{0};
    uint64_t write_bytes_sec{0};
    uint32_t iops{0};
};

struct NetworkMetrics {
    uint64_t rx_bytes_sec{0};
    uint64_t tx_bytes_sec{0};
};

struct ProcessInfo {
    int32_t pid{0};
    int32_t ppid{0};
    std::string name;
    std::string user;
    uint64_t rss_bytes{0};
    char state{'R'};
};

struct SystemSnapshot {
    std::vector<CpuCoreMetrics> cpu_cores;
    MemoryMetrics memory;
    DiskMetrics disk;
    NetworkMetrics network;
    std::vector<ProcessInfo> processes;
    float cpu_temperature_c{45.0f};
    std::string gpu_vendor{"Intel/AMD/NVIDIA"};
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_METRICS_SNAPSHOT_HPP
