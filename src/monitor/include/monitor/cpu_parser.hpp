#ifndef TINEXUS_MONITOR_CPU_PARSER_HPP
#define TINEXUS_MONITOR_CPU_PARSER_HPP

#include "monitor/metrics_snapshot.hpp"
#include <vector>

namespace tinexus::monitor {

struct CpuStatRaw {
    uint64_t user{0};
    uint64_t nice{0};
    uint64_t system{0};
    uint64_t idle{0};
    uint64_t iowait{0};
    uint64_t irq{0};
    uint64_t softirq{0};
    uint64_t steal{0};
};

class CpuParser {
public:
    CpuParser() = default;
    ~CpuParser() = default;

    std::vector<CpuCoreMetrics> parse_cpu_usage(float& out_aggregate_percent);

private:
    CpuStatRaw m_prev_aggregate;
    bool m_has_prev_aggregate{false};
    std::vector<CpuStatRaw> m_prev_stats;
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_CPU_PARSER_HPP
