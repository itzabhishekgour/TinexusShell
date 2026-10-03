#ifndef TINEXUS_MONITOR_PROCESS_TREE_HPP
#define TINEXUS_MONITOR_PROCESS_TREE_HPP

#include "monitor/metrics_snapshot.hpp"
#include <vector>
#include <unordered_map>
#include <chrono>
#include <string>
#include <sys/types.h>

namespace tinexus::monitor {

class ProcessTree {
public:
    ProcessTree();
    ~ProcessTree() = default;

    std::vector<ProcessInfo> discover_processes();

private:
    struct PrevProcStats {
        uint64_t utime_stime{0};
        uint64_t read_bytes{0};
        uint64_t write_bytes{0};
    };

    long m_page_size{4096};
    uid_t m_current_uid{0};
    uint64_t m_prev_total_jiffies{0};
    std::chrono::steady_clock::time_point m_prev_scan_time{};
    bool m_has_prev_scan{false};

    std::unordered_map<pid_t, PrevProcStats> m_prev_proc_stats;
    std::unordered_map<uid_t, std::string> m_uid_cache;

    uint64_t read_total_jiffies();
    std::string resolve_username(uid_t uid);
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_PROCESS_TREE_HPP
