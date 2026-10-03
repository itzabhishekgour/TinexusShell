#ifndef TINEXUS_MONITOR_RESOURCE_MONITOR_HPP
#define TINEXUS_MONITOR_RESOURCE_MONITOR_HPP

#include "monitor/metrics_snapshot.hpp"
#include "monitor/cpu_parser.hpp"
#include "monitor/disk_parser.hpp"
#include "monitor/network_parser.hpp"
#include "monitor/process_tree.hpp"
#include <deque>
#include <mutex>
#include <vector>

namespace tinexus::monitor {

class ResourceMonitor {
public:
    static ResourceMonitor& instance() noexcept;

    ResourceMonitor() = default;
    ~ResourceMonitor() = default;

    SystemSnapshot collect_snapshot();
    [[nodiscard]] std::vector<SystemSnapshot> history() const;

private:
    CpuParser m_cpu_parser;
    DiskParser m_disk_parser;
    NetworkParser m_network_parser;
    ProcessTree m_process_tree;

    mutable std::mutex m_mutex;
    std::deque<SystemSnapshot> m_history_ring;
    size_t m_history_capacity{60};
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_RESOURCE_MONITOR_HPP
