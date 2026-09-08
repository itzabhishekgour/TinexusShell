#ifndef TINEXUS_MONITOR_NETWORK_PARSER_HPP
#define TINEXUS_MONITOR_NETWORK_PARSER_HPP

#include "monitor/metrics_snapshot.hpp"
#include <chrono>

namespace tinexus::monitor {

class NetworkParser {
public:
    NetworkParser() = default;
    ~NetworkParser() = default;

    NetworkMetrics parse_network();

private:
    uint64_t m_prev_rx_bytes{0};
    uint64_t m_prev_tx_bytes{0};
    std::chrono::steady_clock::time_point m_prev_time{};
    bool m_has_prev{false};
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_NETWORK_PARSER_HPP
