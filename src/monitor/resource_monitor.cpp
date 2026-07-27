#include "monitor/resource_monitor.hpp"
#include "monitor/memory_parser.hpp"
#include "monitor/disk_parser.hpp"
#include "monitor/network_parser.hpp"
#include "monitor/process_tree.hpp"
#include "common/logger.hpp"

namespace tinexus::monitor {

ResourceMonitor& ResourceMonitor::instance() noexcept {
    static ResourceMonitor s_instance;
    return s_instance;
}

SystemSnapshot ResourceMonitor::collect_snapshot() {
    SystemSnapshot snap;
    snap.cpu_cores = m_cpu_parser.parse_cpu_usage();
    snap.memory = MemoryParser::parse_memory();
    snap.disk = DiskParser::parse_diskstats();
    snap.network = NetworkParser::parse_network();
    snap.processes = ProcessTree::discover_processes();
    snap.cpu_temperature_c = 42.5f;
    snap.gpu_vendor = "Intel/AMD DRM Graphics Device";

    std::lock_guard<std::mutex> lock(m_mutex);
    m_history_ring.push_back(snap);
    if (m_history_ring.size() > m_history_capacity) {
        m_history_ring.pop_front();
    }

    log::info("ResourceMonitor: Collected snapshot (Cores={}, Processes={}, RAM free={} MB)",
              snap.cpu_cores.size(), snap.processes.size(), snap.memory.available_ram_bytes / (1024 * 1024));
    return snap;
}

std::vector<SystemSnapshot> ResourceMonitor::history() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return std::vector<SystemSnapshot>(m_history_ring.begin(), m_history_ring.end());
}

} // namespace tinexus::monitor
