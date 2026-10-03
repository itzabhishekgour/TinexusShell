#include "monitor/network_parser.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace tinexus::monitor {

NetworkMetrics NetworkParser::parse_network() {
    NetworkMetrics net;
    auto now = std::chrono::steady_clock::now();

    std::ifstream file("/proc/net/dev");
    if (!file.is_open()) return net;

    std::string line;
    // Skip two header lines
    std::getline(file, line);
    std::getline(file, line);

    uint64_t total_rx_bytes = 0;
    uint64_t total_tx_bytes = 0;
    uint64_t max_iface_rx = 0;
    std::string active_iface;

    while (std::getline(file, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string iface = line.substr(0, colon);
        // Trim spaces
        auto start = iface.find_first_not_of(" \t");
        if (start != std::string::npos) iface = iface.substr(start);

        if (iface == "lo") continue; // Skip loopback

        std::istringstream ss(line.substr(colon + 1));
        uint64_t rx_bytes, rx_packets, rx_errs, rx_drop, rx_fifo, rx_frame, rx_compressed, rx_multicast;
        uint64_t tx_bytes;

        if (ss >> rx_bytes >> rx_packets >> rx_errs >> rx_drop >> rx_fifo >> rx_frame >> rx_compressed >> rx_multicast >> tx_bytes) {
            total_rx_bytes += rx_bytes;
            total_tx_bytes += tx_bytes;

            if (rx_bytes > max_iface_rx) {
                max_iface_rx = rx_bytes;
                active_iface = iface;
            }
        }
    }

    net.active_interface = active_iface.empty() ? "eth0" : active_iface;

    if (m_has_prev) {
        double elapsed_sec = std::chrono::duration<double>(now - m_prev_time).count();
        if (elapsed_sec > 0.05) {
            uint64_t d_rx = (total_rx_bytes >= m_prev_rx_bytes)
                                ? (total_rx_bytes - m_prev_rx_bytes)
                                : 0;
            uint64_t d_tx = (total_tx_bytes >= m_prev_tx_bytes)
                                ? (total_tx_bytes - m_prev_tx_bytes)
                                : 0;

            net.rx_bytes_sec = static_cast<uint64_t>(static_cast<double>(d_rx) / elapsed_sec);
            net.tx_bytes_sec = static_cast<uint64_t>(static_cast<double>(d_tx) / elapsed_sec);
        }
    }

    m_prev_rx_bytes = total_rx_bytes;
    m_prev_tx_bytes = total_tx_bytes;
    m_prev_time = now;
    m_has_prev = true;

    return net;
}

} // namespace tinexus::monitor
