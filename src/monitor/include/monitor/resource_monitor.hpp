#ifndef TINEXUS_MONITOR_RESOURCE_MONITOR_HPP
#define TINEXUS_MONITOR_RESOURCE_MONITOR_HPP

#include "monitor/metrics_snapshot.hpp"
#include "monitor/cpu_parser.hpp"
#include <deque>
#include <mutex>

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
    mutable std::mutex m_mutex;
    std::deque<SystemSnapshot> m_history_ring;
    size_t m_history_capacity{60};
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_RESOURCE_MONITOR_HPP
