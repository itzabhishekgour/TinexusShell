#ifndef TINEXUS_MONITOR_GPU_PARSER_HPP
#define TINEXUS_MONITOR_GPU_PARSER_HPP

#include "monitor/metrics_snapshot.hpp"
#include <string>

namespace tinexus::monitor {

class GpuParser {
public:
    static GpuMetrics parse_gpu(const std::string& base_drm = "/sys/class/drm");
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_GPU_PARSER_HPP
