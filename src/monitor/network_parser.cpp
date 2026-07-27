#include "monitor/network_parser.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>

namespace tinexus::monitor {

NetworkMetrics NetworkParser::parse_network() {
    NetworkMetrics net;
    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) return net;

    std::string line;
    // Skip header lines
    std::getline(file, line);
    std::getline(file, line);

    while (std::getline(file, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string iface = line.substr(0, colon);
        std::istringstream ss(line.substr(colon + 1));

        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
        uint64_t tx_bytes;

        if (ss >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast >> tx_bytes) {
            if (iface.find("lo") == std::string::npos) {
                net.rx_bytes_sec += rx_bytes;
                net.tx_bytes_sec += tx_bytes;
            }
        }
    }
    return net;
}

} // namespace tinexus::monitor
