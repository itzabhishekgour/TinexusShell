#include "common/NetUtils.hpp"

#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>

namespace fs = std::filesystem;

namespace tinexus::net {

namespace {

bool is_rfkill_blocked(const std::string& base_rfkill) {
    std::error_code ec;
    if (!fs::exists(base_rfkill, ec)) {
        return false;
    }

    try {
        for (const auto& entry : fs::directory_iterator(base_rfkill, ec)) {
            if (!entry.is_directory()) continue;
            
            // Read type
            std::string type;
            std::ifstream type_file(entry.path() / "type");
            if (type_file >> type && type == "wlan") {
                // Check soft block
                std::ifstream soft_file(entry.path() / "soft");
                int soft_val = 0;
                if (soft_file >> soft_val && soft_val == 1) {
                    return true;
                }

                // Check hard block
                std::ifstream hard_file(entry.path() / "hard");
                int hard_val = 0;
                if (hard_file >> hard_val && hard_val == 1) {
                    return true;
                }
            }
        }
    } catch (...) {
        return false;
    }
    return false;
}

std::string find_wifi_iface_once(const std::string& base_net) {
    std::error_code ec;
    if (!fs::exists(base_net, ec)) {
        return "";
    }

    std::vector<std::string> candidates;
    try {
        for (const auto& entry : fs::directory_iterator(base_net, ec)) {
            std::string ifname = entry.path().filename().string();
            if (ifname == "lo") continue;

            // Must be physical device (has device symlink)
            if (!fs::exists(entry.path() / "device", ec)) {
                continue;
            }

            // Must have wireless extensions or mac80211/cfg80211 phy
            bool has_wireless = fs::exists(entry.path() / "wireless", ec) ||
                                fs::exists(entry.path() / "phy80211", ec);
            if (has_wireless) {
                candidates.push_back(ifname);
            }
        }
    } catch (...) {
        return "";
    }

    if (candidates.empty()) {
        return "";
    }

    // Sort to prioritize predictable names: wlan0, wlo1, wlp*
    std::sort(candidates.begin(), candidates.end());
    return candidates.front();
}

} // namespace

WifiInterfaceResult probe_primary_wifi_interface(
    const std::string& base_sysfs_net,
    const std::string& base_sysfs_rfkill,
    int timeout_ms
) {
    int elapsed_ms = 0;
    constexpr int POLL_INTERVAL_MS = 250;

    while (true) {
        std::string iface = find_wifi_iface_once(base_sysfs_net);
        if (!iface.empty()) {
            // Found interface, check rfkill state
            if (is_rfkill_blocked(base_sysfs_rfkill)) {
                return WifiInterfaceResult{WifiHardwareState::BlockedRfkill, iface};
            }
            return WifiInterfaceResult{WifiHardwareState::Available, iface};
        }

        if (elapsed_ms >= timeout_ms) {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
        elapsed_ms += POLL_INTERVAL_MS;
    }

    // Hardware was not detected within timeout
    return WifiInterfaceResult{WifiHardwareState::NotDetected, ""};
}

std::vector<PhysicalInterfaceInfo> get_physical_interfaces(
    const std::string& base_sysfs_net
) {
    std::vector<PhysicalInterfaceInfo> list;
    std::error_code ec;

    if (!fs::exists(base_sysfs_net, ec)) {
        return list;
    }

    // Gather IP addresses from getifaddrs
    struct ifaddrs* ifaddr = nullptr;
    std::vector<std::pair<std::string, std::string>> ip_map;
    if (getifaddrs(&ifaddr) == 0 && ifaddr != nullptr) {
        for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (!ifa->ifa_name) continue;
            auto* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            char ip_buf[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &sa->sin_addr, ip_buf, sizeof(ip_buf))) {
                ip_map.emplace_back(ifa->ifa_name, ip_buf);
            }
        }
        freeifaddrs(ifaddr);
    }

    try {
        for (const auto& entry : fs::directory_iterator(base_sysfs_net, ec)) {
            std::string ifname = entry.path().filename().string();
            if (ifname == "lo") continue;

            // Physical interface filter: must have "device" symlink in sysfs
            if (!fs::exists(entry.path() / "device", ec)) {
                continue;
            }

            PhysicalInterfaceInfo info;
            info.name = ifname;
            info.is_wireless = fs::exists(entry.path() / "wireless", ec) ||
                               fs::exists(entry.path() / "phy80211", ec);

            // Operstate
            std::ifstream op_f(entry.path() / "operstate");
            std::string op_st;
            if (op_f >> op_st) {
                info.operstate = op_st;
            }

            // MAC Address
            std::ifstream mac_f(entry.path() / "address");
            std::string mac_st;
            if (mac_f >> mac_st) {
                info.mac_addr = mac_st;
            }

            // IP Address
            for (const auto& [name, ip] : ip_map) {
                if (name == ifname) {
                    info.ip4_addr = ip;
                    break;
                }
            }

            // Statistics (rx/tx bytes)
            std::ifstream rx_f(entry.path() / "statistics" / "rx_bytes");
            uint64_t rx_bytes = 0;
            if (rx_f >> rx_bytes) {
                info.rx_mb = rx_bytes / (1024 * 1024);
            }

            std::ifstream tx_f(entry.path() / "statistics" / "tx_bytes");
            uint64_t tx_bytes = 0;
            if (tx_f >> tx_bytes) {
                info.tx_mb = tx_bytes / (1024 * 1024);
            }

            list.push_back(std::move(info));
        }
    } catch (...) {}

    std::sort(list.begin(), list.end(), [](const PhysicalInterfaceInfo& a, const PhysicalInterfaceInfo& b) {
        if (a.is_wireless != b.is_wireless) return a.is_wireless > b.is_wireless;
        return a.name < b.name;
    });

    return list;
}

} // namespace tinexus::net
