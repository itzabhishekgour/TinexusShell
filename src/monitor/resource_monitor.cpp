#include "monitor/resource_monitor.hpp"
#include "monitor/memory_parser.hpp"
#include "monitor/power_parser.hpp"
#include "monitor/gpu_parser.hpp"
#include "common/logger.hpp"

namespace tinexus::monitor {

ResourceMonitor& ResourceMonitor::instance() noexcept {
    static ResourceMonitor s_instance;
    return s_instance;
}

SystemSnapshot ResourceMonitor::collect_snapshot() {
    SystemSnapshot snap;
    snap.cpu_cores = m_cpu_parser.parse_cpu_usage(snap.cpu_aggregate_usage_percent);
    snap.memory = MemoryParser::parse_memory();
    snap.disk = m_disk_parser.parse_diskstats();
    snap.network = m_network_parser.parse_network();
    snap.power = PowerParser::parse_power();
    snap.gpu = GpuParser::parse_gpu();
    snap.processes = m_process_tree.discover_processes();
    snap.cpu_temperature_c = 42.5f;

    std::lock_guard<std::mutex> lock(m_mutex);
    m_history_ring.push_back(snap);
    if (m_history_ring.size() > m_history_capacity) {
        m_history_ring.pop_front();
    }

    return snap;
}

std::vector<SystemSnapshot> ResourceMonitor::history() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<SystemSnapshot>(m_history_ring.begin(), m_history_ring.end());
}

} // namespace tinexus::monitor
