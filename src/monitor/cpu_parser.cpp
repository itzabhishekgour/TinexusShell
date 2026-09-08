#include "monitor/cpu_parser.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace tinexus::monitor {

std::vector<CpuCoreMetrics> CpuParser::parse_cpu_usage(float& out_aggregate_percent) {
    std::vector<CpuCoreMetrics> metrics;
    out_aggregate_percent = 0.0f;

    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        log::error("CpuParser: Failed to open /proc/stat");
        return metrics;
    }

    std::string line;
    uint32_t core_idx = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        // 1. Overall aggregate CPU line: starts with "cpu "
        if (line.rfind("cpu ", 0) == 0) {
            std::istringstream ss(line);
            std::string label;
            CpuStatRaw curr;
            ss >> label >> curr.user >> curr.nice >> curr.system >> curr.idle
               >> curr.iowait >> curr.irq >> curr.softirq >> curr.steal;

            if (m_has_prev_aggregate) {
                uint64_t prev_idle = m_prev_aggregate.idle + m_prev_aggregate.iowait;
                uint64_t curr_idle = curr.idle + curr.iowait;
                uint64_t prev_non_idle = m_prev_aggregate.user + m_prev_aggregate.nice + m_prev_aggregate.system
                                       + m_prev_aggregate.irq + m_prev_aggregate.softirq + m_prev_aggregate.steal;
                uint64_t curr_non_idle = curr.user + curr.nice + curr.system
                                       + curr.irq + curr.softirq + curr.steal;

                uint64_t totald = (curr_idle + curr_non_idle) - (prev_idle + prev_non_idle);
                uint64_t idled = curr_idle - prev_idle;
                if (totald > 0) {
                    out_aggregate_percent = std::clamp(
                        (static_cast<float>(totald - idled) / static_cast<float>(totald)) * 100.0f,
                        0.0f, 100.0f);
                }
            }
            m_prev_aggregate = curr;
            m_has_prev_aggregate = true;
            continue;
        }

        // 2. Per-core lines: "cpu0", "cpu1", ...
        if (line.rfind("cpu", 0) == 0 && line.length() > 3 && std::isdigit(line[3])) {
            std::istringstream ss(line);
            std::string cpu_label;
            CpuStatRaw curr;
            ss >> cpu_label >> curr.user >> curr.nice >> curr.system >> curr.idle
               >> curr.iowait >> curr.irq >> curr.softirq >> curr.steal;

            float usage = 0.0f;
            if (core_idx < m_prev_stats.size()) {
                const auto& prev = m_prev_stats[core_idx];
                uint64_t prev_idle = prev.idle + prev.iowait;
                uint64_t curr_idle = curr.idle + curr.iowait;
                uint64_t prev_non_idle = prev.user + prev.nice + prev.system + prev.irq + prev.softirq + prev.steal;
                uint64_t curr_non_idle = curr.user + curr.nice + curr.system + curr.irq + curr.softirq + curr.steal;

                uint64_t totald = (curr_idle + curr_non_idle) - (prev_idle + prev_non_idle);
                uint64_t idled = curr_idle - prev_idle;
                if (totald > 0) {
                    usage = std::clamp(
                        (static_cast<float>(totald - idled) / static_cast<float>(totald)) * 100.0f,
                        0.0f, 100.0f);
                }
            }

            if (core_idx >= m_prev_stats.size()) {
                m_prev_stats.push_back(curr);
            } else {
                m_prev_stats[core_idx] = curr;
            }

            CpuCoreMetrics core;
            core.core_id = core_idx++;
            core.usage_percent = usage;
            metrics.push_back(core);
        }
    }

    return metrics;
}

} // namespace tinexus::monitor
