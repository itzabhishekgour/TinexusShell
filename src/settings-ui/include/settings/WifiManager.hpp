#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <memory>

namespace tinexus::settings_ui {

struct WifiNetwork {
    std::string bssid;
    std::string ssid;
    int signal_dbm{-100};
    int signal_bars{1};      // 1 to 4 bars
    int frequency_mhz{0};    // e.g. 2412 or 5180
    bool is_secured{true};
    std::string security_str{"WPA2-Personal"};
    bool is_connected{false};
};

class WifiManager {
public:
    static WifiManager& instance();

    WifiManager();
    ~WifiManager();

    // Prevent copies
    WifiManager(const WifiManager&) = delete;
    WifiManager& operator=(const WifiManager&) = delete;

    bool is_wifi_enabled() const noexcept { return m_wifi_enabled.load(); }
    void set_wifi_enabled(bool enabled);

    void trigger_scan();
    bool is_scanning() const noexcept { return m_is_scanning.load(); }

    std::vector<WifiNetwork> get_networks();

    void connect(const std::string& ssid, const std::string& password);
    void disconnect();

    bool is_connecting() const noexcept { return m_is_connecting.load(); }
    std::string get_connecting_ssid() const;
    std::string get_connected_ssid() const;
    std::string get_ip_address() const;
    std::string get_status_message() const;
    std::string get_active_interface() const;
    int get_connected_signal_bars() const;

    // Testable command builders for headless verification
    static std::vector<std::string> build_wpa_network_commands(const std::string& net_id,
                                                               const std::string& ssid,
                                                               const std::string& password);
    static std::vector<std::string> build_dhcp_client_args(const std::string& iface,
                                                           const std::string& hostname);

private:
    std::atomic<bool> m_wifi_enabled{true};
    std::atomic<bool> m_is_scanning{false};
    std::atomic<bool> m_is_connecting{false};

    mutable std::mutex m_mutex;
    std::vector<WifiNetwork> m_networks;
    std::string m_connected_ssid;
    std::string m_connecting_ssid;
    std::string m_ip_address;
    std::string m_status_message;
    std::string m_active_iface;
    int m_connected_signal_bars{0};

    std::thread m_scan_thread;
    std::thread m_connect_thread;

    bool has_wifi_hardware() const noexcept;
    bool ensure_wpa_supplicant_running();
    std::string send_wpa_command(const std::string& cmd);
    void refresh_status_internal();
    void parse_scan_results(const std::string& raw);
    void scan_worker();
    void connect_worker(std::string ssid, std::string password);
};

} // namespace tinexus::settings_ui
