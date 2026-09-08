#ifndef TINEXUS_MONITOR_DISK_PARSER_HPP
#define TINEXUS_MONITOR_DISK_PARSER_HPP

#include "monitor/metrics_snapshot.hpp"
#include <chrono>

namespace tinexus::monitor {

class DiskParser {
public:
    DiskParser() = default;
    ~DiskParser() = default;

    DiskMetrics parse_diskstats();

private:
    uint64_t m_prev_sectors_read{0};
    uint64_t m_prev_sectors_written{0};
    uint64_t m_prev_io_count{0};
    std::chrono::steady_clock::time_point m_prev_time{};
    bool m_has_prev{false};
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_DISK_PARSER_HPP
