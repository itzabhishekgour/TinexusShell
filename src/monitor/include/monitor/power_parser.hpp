#ifndef TINEXUS_MONITOR_POWER_PARSER_HPP
#define TINEXUS_MONITOR_POWER_PARSER_HPP

#include "monitor/metrics_snapshot.hpp"
#include <string>

namespace tinexus::monitor {

class PowerParser {
public:
    static PowerMetrics parse_power(const std::string& base_sysfs = "/sys/class/power_supply");
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_POWER_PARSER_HPP
