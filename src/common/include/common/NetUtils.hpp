#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace tinexus::net {

enum class WifiHardwareState {
    Available,
    BlockedRfkill,
    NotDetected
};

struct WifiInterfaceResult {
    WifiHardwareState state{WifiHardwareState::NotDetected};
    std::string iface_name{};
};

struct PhysicalInterfaceInfo {
    std::string name;
    bool is_wireless{false};
    std::string operstate{"down"};
    std::string ip4_addr{};
    std::string mac_addr{};
    uint64_t rx_mb{0};
    uint64_t tx_mb{0};
};

/**
 * @brief Atomically probes for the primary physical Wi-Fi network interface in a single pass.
 * 
 * Inspects base_sysfs_net for interfaces having a physical hardware link (device symlink)
 * AND wireless capabilities (wireless/ or phy80211/ sysfs nodes).
 * 
 * Also checks base_sysfs_rfkill for any wlan rfkill blocks (both soft and hard blocks).
 * 
 * @param base_sysfs_net Path to sysfs net directory (default: "/sys/class/net")
 * @param base_sysfs_rfkill Path to sysfs rfkill directory (default: "/sys/class/rfkill")
 * @param timeout_ms Optional polling timeout in milliseconds (polls every 250ms if > 0)
 * @return WifiInterfaceResult containing state (Available, BlockedRfkill, NotDetected) and iface_name
 */
WifiInterfaceResult probe_primary_wifi_interface(
    const std::string& base_sysfs_net = "/sys/class/net",
    const std::string& base_sysfs_rfkill = "/sys/class/rfkill",
    int timeout_ms = 0
);

/**
 * @brief Enumerates physical network adapters, excluding loopback and virtual interfaces (docker, veth, etc.).
 * 
 * Requires the existence of "<base_sysfs_net>/<iface>/device" symlink to verify real hardware NICs.
 * 
 * @param base_sysfs_net Path to sysfs net directory (default: "/sys/class/net")
 * @return List of physical interfaces with operstate, IP, MAC, and RX/TX statistics
 */
std::vector<PhysicalInterfaceInfo> get_physical_interfaces(
    const std::string& base_sysfs_net = "/sys/class/net"
);

} // namespace tinexus::net
