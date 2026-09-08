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
    uint64_t used_ram_bytes{0};
    uint64_t free_ram_bytes{0};
    uint64_t buffers_bytes{0};
    uint64_t cached_bytes{0};
    uint64_t total_swap_bytes{0};
    uint64_t free_swap_bytes{0};
    uint64_t used_swap_bytes{0};
};

struct DiskMetrics {
    uint64_t read_bytes_sec{0};
    uint64_t write_bytes_sec{0};
    uint32_t iops{0};
    uint64_t root_total_bytes{0};
    uint64_t root_used_bytes{0};
    uint64_t root_free_bytes{0};
};

struct NetworkMetrics {
    uint64_t rx_bytes_sec{0};
    uint64_t tx_bytes_sec{0};
    std::string active_interface;
};

struct PowerMetrics {
    int battery_percent{100};
    std::string power_status{"Full"};
    float power_watts{0.0f};
    bool has_battery{false};
};

struct GpuMetrics {
    std::string vendor{"Intel"};
    std::string device_name{"Integrated Graphics"};
    std::string driver{"i915"};
    std::string telemetry_status{"Telemetry requires vendor driver (NVML/Intel PMU)"};
    float busy_percent{-1.0f}; // -1 if unavailable
};

struct ProcessInfo {
    int32_t pid{0};
    int32_t ppid{0};
    uint32_t uid{0};
    std::string name;
    std::string user;
    uint64_t rss_bytes{0};
    float cpu_percent{0.0f};
    uint64_t read_bytes_sec{0};
    uint64_t write_bytes_sec{0};
    bool has_io_permission{true};
    char state{'S'};
    std::string state_str{"Sleeping"};
    std::string energy_impact{"Low"}; // Heuristic
    bool is_system_daemon{false};
};

struct SystemSnapshot {
    float cpu_aggregate_usage_percent{0.0f};
    std::vector<CpuCoreMetrics> cpu_cores;
    MemoryMetrics memory;
    DiskMetrics disk;
    NetworkMetrics network;
    PowerMetrics power;
    GpuMetrics gpu;
    std::vector<ProcessInfo> processes;
    float cpu_temperature_c{45.0f};
    std::chrono::system_clock::time_point timestamp{std::chrono::system_clock::now()};
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_METRICS_SNAPSHOT_HPP
