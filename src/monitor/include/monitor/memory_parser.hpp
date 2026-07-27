#ifndef TINEXUS_MONITOR_MEMORY_PARSER_HPP
#define TINEXUS_MONITOR_MEMORY_PARSER_HPP

#include "monitor/metrics_snapshot.hpp"

namespace tinexus::monitor {

class MemoryParser {
public:
    static MemoryMetrics parse_memory();
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_MEMORY_PARSER_HPP
