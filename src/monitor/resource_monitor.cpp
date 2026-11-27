#include "monitor/resource_monitor.hpp"
#include "monitor/memory_parser.hpp"
#include "monitor/power_parser.hpp"
#include "monitor/gpu_parser.hpp"
#include "common/logger.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace tinexus::monitor {

namespace {

float read_sysfs_millidegrees(const fs::path& p) {
    std::ifstream f(p);
    long raw = 0;
    if (!(f >> raw)) return -1.0f;
    float deg_c = (raw > 1000) ? (static_cast<float>(raw) / 1000.0f) : static_cast<float>(raw);
    if (deg_c >= 5.0f && deg_c <= 125.0f) {
        return deg_c;
    }
    return -1.0f;
}

float read_cpu_temperature_c() {
    float best_temp = -1.0f;
    int best_priority = -1;

    std::error_code ec;
    // 1. Prefer Linux hwmon CPU package / core sensors
    if (fs::exists("/sys/class/hwmon", ec)) {
        for (const auto& hw : fs::directory_iterator("/sys/class/hwmon", ec)) {
            if (!hw.is_directory(ec)) continue;
            std::string sensor_name;
            if (std::ifstream nf(hw.path() / "name"); nf.is_open()) {
                std::getline(nf, sensor_name);
            }
            int priority = 1;
            if (sensor_name.find("coretemp") != std::string::npos ||
                sensor_name.find("k10temp") != std::string::npos ||
                sensor_name.find("zenpower") != std::string::npos) {
                priority = 4;
            } else if (sensor_name.find("acpitz") != std::string::npos ||
                       sensor_name.find("x86_pkg_temp") != std::string::npos ||
                       sensor_name.find("cpu") != std::string::npos) {
                priority = 3;
            }
            for (const auto& entry : fs::directory_iterator(hw.path(), ec)) {
                const std::string fname = entry.path().filename().string();
                if (fname.rfind("temp", 0) == 0 && fname.ends_with("_input")) {
                    float t = read_sysfs_millidegrees(entry.path());
                    if (t > 0.0f && (priority > best_priority || (priority == best_priority && t > best_temp))) {
                        best_priority = priority;
                        best_temp = t;
                    }
                }
            }
        }
    }

    // 2. Fallback to /sys/class/thermal/thermal_zone*/temp
    if (best_temp < 0.0f && fs::exists("/sys/class/thermal", ec)) {
        for (const auto& tz : fs::directory_iterator("/sys/class/thermal", ec)) {
            if (!tz.is_directory(ec)) continue;
            if (tz.path().filename().string().rfind("thermal_zone", 0) != 0) continue;
            std::string type_name;
            if (std::ifstream tf(tz.path() / "type"); tf.is_open()) {
                std::getline(tf, type_name);
            }
            int priority = 1;
            if (type_name.find("x86_pkg_temp") != std::string::npos ||
                type_name.find("TCPU") != std::string::npos ||
                type_name.find("cpu") != std::string::npos) {
                priority = 3;
            } else if (type_name.find("acpitz") != std::string::npos) {
                priority = 2;
            }
            float t = read_sysfs_millidegrees(tz.path() / "temp");
            if (t > 0.0f && (priority > best_priority || (priority == best_priority && t > best_temp))) {
                best_priority = priority;
                best_temp = t;
            }
        }
    }

    return best_temp;
}

} // namespace

ResourceMonitor& ResourceMonitor::instance() noexcept {
    static ResourceMonitor s_instance;
    return s_instance;
}

SystemSnapshot ResourceMonitor::collect_snapshot(bool include_processes) {
    SystemSnapshot snap;
    snap.cpu_cores = m_cpu_parser.parse_cpu_usage(snap.cpu_aggregate_usage_percent);
    snap.memory = MemoryParser::parse_memory();
    snap.disk = m_disk_parser.parse_diskstats();
    snap.network = m_network_parser.parse_network();
    snap.power = PowerParser::parse_power();
    snap.gpu = GpuParser::parse_gpu();
    if (include_processes) {
        snap.processes = m_process_tree.discover_processes();
    }
    snap.cpu_temperature_c = read_cpu_temperature_c();

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
