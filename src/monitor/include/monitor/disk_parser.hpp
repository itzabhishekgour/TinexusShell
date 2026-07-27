#ifndef TINEXUS_MONITOR_DISK_PARSER_HPP
#define TINEXUS_MONITOR_DISK_PARSER_HPP

#include "monitor/metrics_snapshot.hpp"

namespace tinexus::monitor {

class DiskParser {
public:
    static DiskMetrics parse_diskstats();
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_DISK_PARSER_HPP
