#include "settings/WifiManager.hpp"
#include "common/NetUtils.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>

using namespace tinexus::settings_ui;
namespace fs = std::filesystem;

int main() {
    std::cout << "[Contract Test] Starting Wi-Fi backend contract assertions..." << std::endl;

    // ── Contract 1: WPA2/WPA3 Personal Transition Mode Cipher & PMF Contract ─
    {
        std::cout << "[Contract Test] 1. Asserting WPA2/WPA3 transition mode command sequence..." << std::endl;
        auto cmds = WifiManager::build_wpa_network_commands("0", "OPPO F23 5G", "Abhi2002");

        bool has_pairwise_ccmp_only = false;
        bool has_forbidden_pairwise_tkip = false;
        bool has_pmf_optional = false;
        bool has_sae_key_mgmt = false;
        bool has_rsn_proto = false;
        bool has_group_fallback = false;

        for (const auto& cmd : cmds) {
            std::cout << "  -> Generated: " << cmd << std::endl;
            if (cmd == "SET_NETWORK 0 pairwise CCMP") {
                has_pairwise_ccmp_only = true;
            }
            if (cmd.find("pairwise") != std::string::npos && cmd.find("TKIP") != std::string::npos) {
                has_forbidden_pairwise_tkip = true;
            }
            if (cmd == "SET_NETWORK 0 ieee80211w 1") {
                has_pmf_optional = true;
            }
            if (cmd == "SET_NETWORK 0 key_mgmt WPA-PSK WPA-PSK-SHA256 SAE") {
                has_sae_key_mgmt = true;
            }
            if (cmd == "SET_NETWORK 0 proto RSN WPA") {
                has_rsn_proto = true;
            }
            if (cmd == "SET_NETWORK 0 group CCMP TKIP") {
                has_group_fallback = true;
            }
        }

        if (!has_pairwise_ccmp_only) {
            std::cerr << "FAILURE: Pairwise cipher must be strictly CCMP!" << std::endl;
            return 1;
        }
        if (has_forbidden_pairwise_tkip) {
            std::cerr << "FAILURE: TKIP is strictly prohibited alongside PMF in pairwise ciphers!" << std::endl;
            return 1;
        }
        if (!has_pmf_optional) {
            std::cerr << "FAILURE: Protected Management Frames (ieee80211w 1) must be enabled!" << std::endl;
            return 1;
        }
        if (!has_sae_key_mgmt) {
            std::cerr << "FAILURE: Key management must support SAE + WPA-PSK!" << std::endl;
            return 1;
        }
        if (!has_rsn_proto) {
            std::cerr << "FAILURE: Proto RSN WPA must be enabled!" << std::endl;
            return 1;
        }
        if (!has_group_fallback) {
            std::cerr << "FAILURE: Group cipher fallback CCMP TKIP must be enabled!" << std::endl;
            return 1;
        }
        std::cout << "[Contract Test] PASS: WPA2/WPA3 transition mode contracts verified." << std::endl;
    }

    // ── Contract 2: Open Network Security Contract ───────────────────────────
    {
        std::cout << "[Contract Test] 2. Asserting Open network command sequence..." << std::endl;
        auto cmds = WifiManager::build_wpa_network_commands("1", "Public_Airport", "");

        bool has_key_mgmt_none = false;
        bool has_psk = false;

        for (const auto& cmd : cmds) {
            if (cmd == "SET_NETWORK 1 key_mgmt NONE") has_key_mgmt_none = true;
            if (cmd.find("psk") != std::string::npos) has_psk = true;
        }

        if (!has_key_mgmt_none) {
            std::cerr << "FAILURE: Open network must configure key_mgmt NONE!" << std::endl;
            return 1;
        }
        if (has_psk) {
            std::cerr << "FAILURE: Open network must not transmit PSK command!" << std::endl;
            return 1;
        }
        std::cout << "[Contract Test] PASS: Open network contracts verified." << std::endl;
    }

    // ── Contract 3: DHCP Client Hostname Option 12 Contract ──────────────────
    {
        std::cout << "[Contract Test] 3. Asserting udhcpc Option 12 Hostname arguments (-x hostname:Tinexus-Desktop)..." << std::endl;
        auto args = WifiManager::build_dhcp_client_args("wlo1", "Tinexus-Desktop");

        bool has_x_flag = false;
        bool has_option12_hostname = false;
        bool has_interface_wlo1 = false;

        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] == "-x" && i + 1 < args.size() && args[i + 1] == "hostname:Tinexus-Desktop") {
                has_x_flag = true;
                has_option12_hostname = true;
            }
            if (args[i] == "-i" && i + 1 < args.size() && args[i + 1] == "wlo1") {
                has_interface_wlo1 = true;
            }
        }

        if (!has_x_flag || !has_option12_hostname) {
            std::cerr << "FAILURE: udhcpc must be invoked with -x hostname:Tinexus-Desktop (Option 12)!" << std::endl;
            return 1;
        }
        if (!has_interface_wlo1) {
            std::cerr << "FAILURE: udhcpc interface must be passed correctly (wlo1)!" << std::endl;
            return 1;
        }
        std::cout << "[Contract Test] PASS: DHCP client Option 12 hostname contracts verified." << std::endl;
    }

    // ── Contract 4: Centralized Sysfs NetUtils & Hardware Probing ─────────────
    {
        std::cout << "[Contract Test] 4. Testing centralized NetUtils sysfs probing with mock fixtures..." << std::endl;
        fs::path fixture_root = "/tmp/tx_test_sysfs";
        fs::remove_all(fixture_root);
        fs::path mock_net = fixture_root / "net";
        fs::path mock_rfkill = fixture_root / "rfkill";

        fs::create_directories(mock_net);
        fs::create_directories(mock_rfkill);

        // Subtest 4A: Intel AX201 laptop interface "wlo1" with physical device + phy80211
        fs::create_directories(mock_net / "wlo1" / "device");
        fs::create_directories(mock_net / "wlo1" / "phy80211");
        {
            std::ofstream op(mock_net / "wlo1" / "operstate");
            op << "up\n";
            std::ofstream mac(mock_net / "wlo1" / "address");
            mac << "00:11:22:33:44:55\n";
        }

        // Subtest 4B: Virtual adapter "docker0" without device symlink
        fs::create_directories(mock_net / "docker0");
        {
            std::ofstream op(mock_net / "docker0" / "operstate");
            op << "up\n";
        }

        // Subtest 4C: Wired ethernet "eth0" with device symlink (no wireless)
        fs::create_directories(mock_net / "eth0" / "device");
        {
            std::ofstream op(mock_net / "eth0" / "operstate");
            op << "down\n";
            std::ofstream mac(mock_net / "eth0" / "address");
            mac << "aa:bb:cc:dd:ee:ff\n";
        }

        // Assert probe_primary_wifi_interface finds wlo1
        auto res = tinexus::net::probe_primary_wifi_interface(mock_net.string(), mock_rfkill.string(), 0);
        if (res.state != tinexus::net::WifiHardwareState::Available || res.iface_name != "wlo1") {
            std::cerr << "FAILURE: probe_primary_wifi_interface failed to find wlo1!" << std::endl;
            return 1;
        }
        std::cout << "  -> Detected dynamic interface: " << res.iface_name << " (State: Available)" << std::endl;

        // Assert physical adapter filtering: should include wlo1 and eth0, but EXCLUDE docker0
        auto phys = tinexus::net::get_physical_interfaces(mock_net.string());
        bool found_wlo1 = false;
        bool found_eth0 = false;
        bool found_docker0 = false;

        for (const auto& p : phys) {
            std::cout << "  -> Physical adapter: " << p.name << " (Wireless=" << p.is_wireless << ", State=" << p.operstate << ")" << std::endl;
            if (p.name == "wlo1") found_wlo1 = true;
            if (p.name == "eth0") found_eth0 = true;
            if (p.name == "docker0") found_docker0 = true;
        }

        if (!found_wlo1 || !found_eth0) {
            std::cerr << "FAILURE: Physical interfaces must contain wlo1 and eth0!" << std::endl;
            return 1;
        }
        if (found_docker0) {
            std::cerr << "FAILURE: Virtual interface docker0 must be excluded from physical adapters!" << std::endl;
            return 1;
        }

        // Subtest 4D: rfkill soft-block detection
        fs::create_directories(mock_rfkill / "rfkill0");
        {
            std::ofstream t(mock_rfkill / "rfkill0" / "type");
            t << "wlan\n";
            std::ofstream s(mock_rfkill / "rfkill0" / "soft");
            s << "1\n";
            std::ofstream h(mock_rfkill / "rfkill0" / "hard");
            h << "0\n";
        }

        auto rf_res = tinexus::net::probe_primary_wifi_interface(mock_net.string(), mock_rfkill.string(), 0);
        if (rf_res.state != tinexus::net::WifiHardwareState::BlockedRfkill || rf_res.iface_name != "wlo1") {
            std::cerr << "FAILURE: rfkill soft-block was not detected!" << std::endl;
            return 1;
        }
        std::cout << "  -> Detected rfkill soft-block on wlo1 successfully." << std::endl;

        // Subtest 4E: rfkill hard-block detection
        {
            std::ofstream s(mock_rfkill / "rfkill0" / "soft");
            s << "0\n";
            std::ofstream h(mock_rfkill / "rfkill0" / "hard");
            h << "1\n";
        }

        auto rf_hard_res = tinexus::net::probe_primary_wifi_interface(mock_net.string(), mock_rfkill.string(), 0);
        if (rf_hard_res.state != tinexus::net::WifiHardwareState::BlockedRfkill || rf_hard_res.iface_name != "wlo1") {
            std::cerr << "FAILURE: rfkill hard-block was not detected!" << std::endl;
            return 1;
        }
        std::cout << "  -> Detected rfkill hard-block on wlo1 successfully." << std::endl;

        // Subtest 4F: Empty sysfs / no wireless hardware detection
        fs::remove_all(mock_net / "wlo1");
        auto empty_res = tinexus::net::probe_primary_wifi_interface(mock_net.string(), mock_rfkill.string(), 0);
        if (empty_res.state != tinexus::net::WifiHardwareState::NotDetected || !empty_res.iface_name.empty()) {
            std::cerr << "FAILURE: Non-wireless sysfs must report NotDetected!" << std::endl;
            return 1;
        }
        std::cout << "  -> Detected absence of Wi-Fi hardware successfully (NotDetected)." << std::endl;

        fs::remove_all(fixture_root);
        std::cout << "[Contract Test] PASS: Centralized sysfs NetUtils contracts verified." << std::endl;
    }

    std::cout << "\n==================================================" << std::endl;
    std::cout << "  ALL WI-FI BACKEND CONTRACTS PASSED (100%)      " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
