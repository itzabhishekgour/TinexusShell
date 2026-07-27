#ifndef TINEXUS_MONITOR_NETWORK_PARSER_HPP
#define TINEXUS_MONITOR_NETWORK_PARSER_HPP

#include "monitor/metrics_snapshot.hpp"

namespace tinexus::monitor {

class NetworkParser {
public:
    static NetworkMetrics parse_network();
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_NETWORK_PARSER_HPP
