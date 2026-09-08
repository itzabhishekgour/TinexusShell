#include "settings/WifiManager.hpp"
#include "common/logger.hpp"
#include "common/NetUtils.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace tinexus::settings_ui {

namespace {
std::atomic<uint32_t> g_socket_counter{0};

std::string trim_str(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n\"");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n\"");
    return s.substr(start, end - start + 1);
}
} // namespace

WifiManager& WifiManager::instance() {
    static WifiManager s_instance;
    return s_instance;
}

WifiManager::WifiManager() {
    tinexus::log::info("[WifiManager] Initializing native Wi-Fi engine...");
    trigger_scan();
}

WifiManager::~WifiManager() {
    if (m_scan_thread.joinable()) {
        m_scan_thread.join();
    }
    if (m_connect_thread.joinable()) {
        m_connect_thread.join();
    }
}

bool WifiManager::has_wifi_hardware() const noexcept {
    auto res = tinexus::net::probe_primary_wifi_interface("/sys/class/net", "/sys/class/rfkill", 0);
    return res.state != tinexus::net::WifiHardwareState::NotDetected;
}

std::string WifiManager::get_active_interface() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_active_iface.empty()) {
        return m_active_iface;
    }
    auto res = tinexus::net::probe_primary_wifi_interface("/sys/class/net", "/sys/class/rfkill", 0);
    return res.iface_name;
}

void WifiManager::set_wifi_enabled(bool enabled) {
    m_wifi_enabled.store(enabled);
    std::string iface = get_active_interface();
    if (iface.empty()) return;

    if (!enabled) {
        disconnect();
        pid_t p = fork();
        if (p == 0) {
            execl("/bin/ip", "ip", "link", "set", iface.c_str(), "down", nullptr);
            execl("/sbin/ifconfig", "ifconfig", iface.c_str(), "down", nullptr);
            _exit(0);
        }
        if (p > 0) waitpid(p, nullptr, 0);

        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected_ssid.clear();
        m_ip_address.clear();
        m_status_message = "Wi-Fi is turned off";
    } else {
        pid_t p = fork();
        if (p == 0) {
            execl("/bin/ip", "ip", "link", "set", iface.c_str(), "up", nullptr);
            execl("/sbin/ifconfig", "ifconfig", iface.c_str(), "up", nullptr);
            _exit(0);
        }
        if (p > 0) waitpid(p, nullptr, 0);

        trigger_scan();
    }
}

void WifiManager::trigger_scan() {
    if (m_is_scanning.load()) return;

    if (m_scan_thread.joinable()) {
        m_scan_thread.join();
    }

    m_is_scanning.store(true);
    m_scan_thread = std::thread(&WifiManager::scan_worker, this);
}

std::vector<WifiNetwork> WifiManager::get_networks() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_networks;
}

std::string WifiManager::get_connected_ssid() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected_ssid;
}

std::string WifiManager::get_connecting_ssid() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connecting_ssid;
}

std::string WifiManager::get_ip_address() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_ip_address;
}

std::string WifiManager::get_status_message() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_status_message;
}

int WifiManager::get_connected_signal_bars() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connected_signal_bars;
}

void WifiManager::connect(const std::string& ssid, const std::string& password) {
    if (m_is_connecting.load()) return;

    if (m_connect_thread.joinable()) {
        m_connect_thread.join();
    }

    m_is_connecting.store(true);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connecting_ssid = ssid;
        m_status_message = "Connecting to " + ssid + "...";
    }

    m_connect_thread = std::thread(&WifiManager::connect_worker, this, ssid, password);
}

void WifiManager::disconnect() {
    send_wpa_command("DISCONNECT");
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected_ssid.clear();
    m_ip_address.clear();
    m_status_message = "Disconnected";
    for (auto& net : m_networks) {
        net.is_connected = false;
    }
}

bool WifiManager::ensure_wpa_supplicant_running() {
    std::string iface = get_active_interface();
    if (iface.empty()) {
        return false;
    }

    std::string sock_path = "/var/run/wpa_supplicant/" + iface;
    if (std::filesystem::exists(sock_path)) {
        return true;
    }

    // Ensure config directory exists with secure permissions
    try {
        std::filesystem::create_directories("/etc/wpa_supplicant");
        std::filesystem::create_directories("/var/run/wpa_supplicant");
        chmod("/var/run/wpa_supplicant", 0755);

        if (!std::filesystem::exists("/etc/wpa_supplicant/wpa_supplicant.conf")) {
            std::ofstream f("/etc/wpa_supplicant/wpa_supplicant.conf");
            f << "ctrl_interface=/var/run/wpa_supplicant\nupdate_config=1\n";
        }
    } catch (...) {}

    // Bring interface up across all standard tool locations
    pid_t up_p = fork();
    if (up_p == 0) {
        execl("/usr/bin/ip", "ip", "link", "set", iface.c_str(), "up", nullptr);
        execl("/bin/ip", "ip", "link", "set", iface.c_str(), "up", nullptr);
        execl("/sbin/ip", "ip", "link", "set", iface.c_str(), "up", nullptr);
        execl("/usr/sbin/ip", "ip", "link", "set", iface.c_str(), "up", nullptr);
        execl("/bin/busybox", "busybox", "ip", "link", "set", iface.c_str(), "up", nullptr);
        execl("/sbin/ifconfig", "ifconfig", iface.c_str(), "up", nullptr);
        execlp("ip", "ip", "link", "set", iface.c_str(), "up", nullptr);
        _exit(0);
    }
    if (up_p > 0) waitpid(up_p, nullptr, 0);

    // Launch wpa_supplicant daemon with nl80211 preferred driver on detected interface
    tinexus::log::info("[WifiManager] Starting wpa_supplicant daemon on interface '{}'...", iface);
    pid_t wpa_p = fork();
    if (wpa_p == 0) {
        execl("/usr/sbin/wpa_supplicant", "wpa_supplicant", "-B", "-D", "nl80211,wext", "-i", iface.c_str(),
              "-c", "/etc/wpa_supplicant/wpa_supplicant.conf", nullptr);
        execl("/usr/bin/wpa_supplicant", "wpa_supplicant", "-B", "-D", "nl80211,wext", "-i", iface.c_str(),
              "-c", "/etc/wpa_supplicant/wpa_supplicant.conf", nullptr);
        execl("/sbin/wpa_supplicant", "wpa_supplicant", "-B", "-D", "nl80211,wext", "-i", iface.c_str(),
              "-c", "/etc/wpa_supplicant/wpa_supplicant.conf", nullptr);
        execl("/bin/wpa_supplicant", "wpa_supplicant", "-B", "-D", "nl80211,wext", "-i", iface.c_str(),
              "-c", "/etc/wpa_supplicant/wpa_supplicant.conf", nullptr);
        execlp("wpa_supplicant", "wpa_supplicant", "-B", "-D", "nl80211,wext", "-i", iface.c_str(),
               "-c", "/etc/wpa_supplicant/wpa_supplicant.conf", nullptr);
        _exit(127);
    }
    if (wpa_p > 0) waitpid(wpa_p, nullptr, 0);

    // Wait up to 2 seconds for control socket to appear
    for (int i = 0; i < 20; ++i) {
        if (std::filesystem::exists(sock_path)) {
            return true;
        }
        usleep(100000); // 100ms
    }

    return false;
}

std::string WifiManager::send_wpa_command(const std::string& cmd) {
    std::string iface = get_active_interface();
    if (iface.empty()) return "";

    std::string sock_path = "/var/run/wpa_supplicant/" + iface;
    if (!std::filesystem::exists(sock_path)) {
        return "";
    }

    int sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (sock < 0) return "";

    struct sockaddr_un local;
    memset(&local, 0, sizeof(local));
    local.sun_family = AF_UNIX;
    uint32_t count = g_socket_counter.fetch_add(1);
    snprintf(local.sun_path, sizeof(local.sun_path), "/tmp/tx_wpa_%d_%u", getpid(), count);
    unlink(local.sun_path);

    if (bind(sock, reinterpret_cast<struct sockaddr*>(&local), sizeof(local)) < 0) {
        close(sock);
        return "";
    }
    chmod(local.sun_path, 0700);

    struct sockaddr_un remote;
    memset(&remote, 0, sizeof(remote));
    remote.sun_family = AF_UNIX;
    strncpy(remote.sun_path, sock_path.c_str(), sizeof(remote.sun_path) - 1);

    if (::connect(sock, reinterpret_cast<struct sockaddr*>(&remote), sizeof(remote)) < 0) {
        close(sock);
        unlink(local.sun_path);
        return "";
    }

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (send(sock, cmd.data(), cmd.size(), 0) < 0) {
        close(sock);
        unlink(local.sun_path);
        return "";
    }

    char buf[16384];
    ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
    std::string reply;
    if (n > 0) {
        buf[n] = '\0';
        reply = std::string(buf, static_cast<size_t>(n));
    }

    close(sock);
    unlink(local.sun_path);
    return reply;
}

void WifiManager::refresh_status_internal() {
    std::string status = send_wpa_command("STATUS");
    std::string cur_ssid;
    std::string ip;
    std::string iface = get_active_interface();

    std::istringstream iss(status);
    std::string line;
    while (std::getline(iss, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);
        if (k == "ssid") cur_ssid = v;
        else if (k == "ip_address") ip = v;
        else if (k == "wpa_state" && v != "COMPLETED") {
            cur_ssid.clear();
        }
    }

    // If wpa_supplicant didn't report IP, check via getifaddrs on active iface
    if (ip.empty() && !iface.empty()) {
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == 0) {
            for (auto* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
                if (ifa->ifa_name && std::string(ifa->ifa_name) == iface) {
                    auto* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                    char ip_buf[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &sa->sin_addr, ip_buf, sizeof(ip_buf))) {
                        ip = ip_buf;
                        break;
                    }
                }
            }
            freeifaddrs(ifaddr);
        }
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected_ssid = cur_ssid;
    m_ip_address = ip;

    int bars = 0;
    if (!cur_ssid.empty()) {
        for (auto& net : m_networks) {
            if (net.ssid == cur_ssid) {
                net.is_connected = true;
                bars = net.signal_bars;
            } else {
                net.is_connected = false;
            }
        }
        m_status_message = "Connected";
    } else if (m_networks.empty()) {
        m_status_message = "No Wi-Fi networks found";
    } else {
        m_status_message = "Not Connected";
    }
    m_connected_signal_bars = bars;
}

void WifiManager::parse_scan_results(const std::string& raw) {
    std::vector<WifiNetwork> parsed;
    std::istringstream iss(raw);
    std::string line;

    // First line is header: bssid / frequency / signal level / flags / ssid
    if (std::getline(iss, line)) {
        // header consumed
    }

    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::istringstream liness(line);
        std::string bssid, freq_s, level_s, flags, ssid;
        if (!(liness >> bssid >> freq_s >> level_s >> flags)) continue;
        
        // The remainder of the line is the SSID (which can contain spaces)
        std::getline(liness, ssid);
        ssid = trim_str(ssid);
        if (ssid.empty()) continue;

        WifiNetwork net;
        net.bssid = bssid;
        net.ssid = ssid;

        try {
            net.frequency_mhz = std::stoi(freq_s);
        } catch (...) { net.frequency_mhz = 2412; }

        try {
            net.signal_dbm = std::stoi(level_s);
        } catch (...) { net.signal_dbm = -70; }

        // Determine signal bars
        if (net.signal_dbm >= -55) {
            net.signal_bars = 4;
        } else if (net.signal_dbm >= -67) {
            net.signal_bars = 3;
        } else if (net.signal_dbm >= -80) {
            net.signal_bars = 2;
        } else {
            net.signal_bars = 1;
        }

        // Determine security
        if (flags.find("WPA3") != std::string::npos || flags.find("SAE") != std::string::npos) {
            net.is_secured = true;
            net.security_str = "WPA3-Personal";
        } else if (flags.find("WPA2") != std::string::npos || flags.find("WPA") != std::string::npos) {
            net.is_secured = true;
            net.security_str = "WPA2-Personal";
        } else if (flags.find("WEP") != std::string::npos) {
            net.is_secured = true;
            net.security_str = "WEP";
        } else {
            net.is_secured = false;
            net.security_str = "Open";
        }

        parsed.push_back(std::move(net));
    }

    // Deduplicate SSIDs: keep the one with strongest signal
    std::vector<WifiNetwork> deduped;
    for (auto& item : parsed) {
        auto existing = std::find_if(deduped.begin(), deduped.end(),
                                     [&](const WifiNetwork& n) { return n.ssid == item.ssid; });
        if (existing == deduped.end()) {
            deduped.push_back(item);
        } else if (item.signal_dbm > existing->signal_dbm) {
            *existing = item;
        }
    }

    // Sort: Connected first, then by signal strength descending
    std::string cur_connected;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cur_connected = m_connected_ssid;
    }

    for (auto& net : deduped) {
        net.is_connected = (net.ssid == cur_connected);
    }

    std::sort(deduped.begin(), deduped.end(), [](const WifiNetwork& a, const WifiNetwork& b) {
        if (a.is_connected != b.is_connected) return a.is_connected > b.is_connected;
        return a.signal_dbm > b.signal_dbm;
    });

    std::lock_guard<std::mutex> lock(m_mutex);
    m_networks = std::move(deduped);
}

void WifiManager::parse_iw_scan_results(const std::string& iface) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return;

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        execl("/usr/sbin/iw", "iw", "dev", iface.c_str(), "scan", nullptr);
        execl("/usr/bin/iw", "iw", "dev", iface.c_str(), "scan", nullptr);
        execl("/sbin/iw", "iw", "dev", iface.c_str(), "scan", nullptr);
        execl("/bin/iw", "iw", "dev", iface.c_str(), "scan", nullptr);
        execlp("iw", "iw", "dev", iface.c_str(), "scan", nullptr);
        _exit(127);
    }

    close(pipefd[1]);
    std::string raw_output;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        raw_output += buffer;
    }
    close(pipefd[0]);
    if (pid > 0) waitpid(pid, nullptr, 0);

    if (raw_output.empty()) return;

    std::vector<WifiNetwork> parsed;
    std::istringstream iss(raw_output);
    std::string line;

    WifiNetwork cur_net;
    bool in_bss = false;

    while (std::getline(iss, line)) {
        if (line.rfind("BSS ", 0) == 0) {
            if (in_bss && !cur_net.ssid.empty()) {
                parsed.push_back(std::move(cur_net));
            }
            in_bss = true;
            cur_net = WifiNetwork{};
            auto space = line.find(' ', 4);
            auto paren = line.find('(', 4);
            size_t end_bssid = std::min(space, paren);
            if (end_bssid != std::string::npos) {
                cur_net.bssid = line.substr(4, end_bssid - 4);
            }
        } else if (in_bss) {
            auto trimmed = trim_str(line);
            if (trimmed.rfind("SSID: ", 0) == 0) {
                cur_net.ssid = trim_str(trimmed.substr(6));
            } else if (trimmed.rfind("signal: ", 0) == 0) {
                try {
                    auto sig_str = trimmed.substr(8);
                    cur_net.signal_dbm = static_cast<int>(std::stof(sig_str));
                } catch (...) { cur_net.signal_dbm = -70; }
            } else if (trimmed.rfind("freq: ", 0) == 0) {
                try {
                    cur_net.frequency_mhz = std::stoi(trimmed.substr(6));
                } catch (...) { cur_net.frequency_mhz = 2412; }
            } else if (trimmed.find("RSN:") != std::string::npos || trimmed.find("WPA:") != std::string::npos || trimmed.find("Authentication suites:") != std::string::npos) {
                cur_net.is_secured = true;
                if (trimmed.find("SAE") != std::string::npos) cur_net.security_str = "WPA3-Personal";
                else cur_net.security_str = "WPA2-Personal";
            }
        }
    }
    if (in_bss && !cur_net.ssid.empty()) {
        parsed.push_back(std::move(cur_net));
    }

    if (parsed.empty()) return;

    // Calculate signal bars
    for (auto& net : parsed) {
        if (net.signal_dbm >= -55) net.signal_bars = 4;
        else if (net.signal_dbm >= -67) net.signal_bars = 3;
        else if (net.signal_dbm >= -80) net.signal_bars = 2;
        else net.signal_bars = 1;
    }

    // Deduplicate SSIDs keeping strongest signal
    std::vector<WifiNetwork> deduped;
    for (auto& item : parsed) {
        auto existing = std::find_if(deduped.begin(), deduped.end(),
                                     [&](const WifiNetwork& n) { return n.ssid == item.ssid; });
        if (existing == deduped.end()) {
            deduped.push_back(item);
        } else if (item.signal_dbm > existing->signal_dbm) {
            *existing = item;
        }
    }

    // Mark connected if matches
    std::string cur_connected;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        cur_connected = m_connected_ssid;
    }
    for (auto& net : deduped) {
        net.is_connected = (net.ssid == cur_connected);
    }

    std::sort(deduped.begin(), deduped.end(), [](const WifiNetwork& a, const WifiNetwork& b) {
        if (a.is_connected != b.is_connected) return a.is_connected > b.is_connected;
        return a.signal_dbm > b.signal_dbm;
    });

    std::lock_guard<std::mutex> lock(m_mutex);
    m_networks = std::move(deduped);
}

void WifiManager::scan_worker() {
    // Run 3000ms polling probe asynchronously inside this background worker thread.
    // This allows delayed kernel/firmware initialization (e.g. MediaTek MT7921/iwlwifi) without blocking the UI.
    auto hw_res = tinexus::net::probe_primary_wifi_interface("/sys/class/net", "/sys/class/rfkill", 3000);

    if (hw_res.state == tinexus::net::WifiHardwareState::NotDetected) {
        tinexus::log::info("[WifiManager] No physical Wi-Fi hardware detected after 3000ms probe.");
        std::lock_guard<std::mutex> lock(m_mutex);
        m_networks.clear();
        m_active_iface.clear();
        m_status_message = "No Wi-Fi Hardware Detected";
        m_is_scanning.store(false);
        return;
    }

    if (hw_res.state == tinexus::net::WifiHardwareState::BlockedRfkill) {
        tinexus::log::warn("[WifiManager] Wi-Fi adapter '{}' is blocked by rfkill switch/airplane mode.", hw_res.iface_name);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_networks.clear();
        m_active_iface = hw_res.iface_name;
        m_status_message = "Wi-Fi is disabled (Hardware switch / Airplane mode)";
        m_is_scanning.store(false);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active_iface = hw_res.iface_name;
    }

    // Explicitly bring interface up before initiating scan
    {
        pid_t up_p = fork();
        if (up_p == 0) {
            execl("/usr/bin/ip", "ip", "link", "set", hw_res.iface_name.c_str(), "up", nullptr);
            execl("/bin/ip", "ip", "link", "set", hw_res.iface_name.c_str(), "up", nullptr);
            execl("/sbin/ip", "ip", "link", "set", hw_res.iface_name.c_str(), "up", nullptr);
            execl("/usr/sbin/ip", "ip", "link", "set", hw_res.iface_name.c_str(), "up", nullptr);
            execl("/bin/busybox", "busybox", "ip", "link", "set", hw_res.iface_name.c_str(), "up", nullptr);
            execlp("ip", "ip", "link", "set", hw_res.iface_name.c_str(), "up", nullptr);
            _exit(0);
        }
        if (up_p > 0) waitpid(up_p, nullptr, 0);
    }

    ensure_wpa_supplicant_running();
    send_wpa_command("SCAN");
    
    // Allow passive and active 802.11 channel sweep to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));

    std::string raw = send_wpa_command("SCAN_RESULTS");
    if (!raw.empty() && raw.find("bssid") != std::string::npos) {
        parse_scan_results(raw);
    } else {
        // Direct kernel iw scan fallback if wpa_supplicant socket was delayed
        parse_iw_scan_results(hw_res.iface_name);
    }
    refresh_status_internal();
    m_is_scanning.store(false);
}

std::vector<std::string> WifiManager::build_wpa_network_commands(const std::string& net_id,
                                                                 const std::string& ssid,
                                                                 const std::string& password) {
    std::vector<std::string> cmds;
    cmds.push_back("SET_NETWORK " + net_id + " ssid \"" + ssid + "\"");
    if (!password.empty()) {
        cmds.push_back("SET_NETWORK " + net_id + " psk \"" + password + "\"");
        cmds.push_back("SET_NETWORK " + net_id + " key_mgmt WPA-PSK WPA-PSK-SHA256 SAE");
        cmds.push_back("SET_NETWORK " + net_id + " ieee80211w 1"); // 1 = optional PMF (WPA3 transition mode compliant)
        cmds.push_back("SET_NETWORK " + net_id + " proto RSN WPA");
        cmds.push_back("SET_NETWORK " + net_id + " pairwise CCMP"); // CCMP ONLY (Strictly zero TKIP in pairwise!)
        cmds.push_back("SET_NETWORK " + net_id + " group CCMP TKIP"); // Group cipher legacy fallback
    } else {
        cmds.push_back("SET_NETWORK " + net_id + " key_mgmt NONE");
    }
    return cmds;
}

std::vector<std::string> WifiManager::build_dhcp_client_args(const std::string& iface,
                                                             const std::string& hostname) {
    return {
        "-i", iface,
        "-s", "/usr/share/udhcpc/default.script",
        "-x", "hostname:" + hostname,
        "-q", "-n"
    };
}

void WifiManager::connect_worker(std::string ssid, std::string password) {
    std::string iface = get_active_interface();
    if (iface.empty()) {
        auto probe = tinexus::net::probe_primary_wifi_interface("/sys/class/net", "/sys/class/rfkill", 0);
        if (probe.state == tinexus::net::WifiHardwareState::NotDetected) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_is_connecting.store(false);
            m_status_message = "No Wi-Fi Hardware Detected";
            return;
        }
        iface = probe.iface_name;
        std::lock_guard<std::mutex> lock(m_mutex);
        m_active_iface = iface;
    }

    ensure_wpa_supplicant_running();

    send_wpa_command("DISCONNECT");
    send_wpa_command("REMOVE_NETWORK all");

    std::string net_id = trim_str(send_wpa_command("ADD_NETWORK"));
    if (net_id.empty() || net_id.find("FAIL") != std::string::npos) {
        net_id = "0";
    }

    auto net_cmds = build_wpa_network_commands(net_id, ssid, password);
    for (const auto& cmd : net_cmds) {
        send_wpa_command(cmd);
    }

    send_wpa_command("ENABLE_NETWORK " + net_id);
    send_wpa_command("SELECT_NETWORK " + net_id);
    send_wpa_command("REASSOCIATE");

    bool connected = false;
    // Wait up to 15 seconds for 802.11 association and 4-way handshake
    for (int attempt = 0; attempt < 30; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        std::string status = send_wpa_command("STATUS");
        if (status.find("wpa_state=COMPLETED") != std::string::npos) {
            connected = true;
            break;
        }
    }

    if (connected) {
        tinexus::log::info("[WifiManager] Successfully associated with '{}'. Requesting DHCP lease on '{}'...", ssid, iface);
        
        // Run udhcpc via default script across all standard binary paths with hostname Option 12
        pid_t dhcp_p = fork();
        if (dhcp_p == 0) {
            execl("/usr/bin/udhcpc", "udhcpc", "-i", iface.c_str(),
                  "-s", "/usr/share/udhcpc/default.script", "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
            execl("/bin/udhcpc", "udhcpc", "-i", iface.c_str(),
                  "-s", "/usr/share/udhcpc/default.script", "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
            execl("/sbin/udhcpc", "udhcpc", "-i", iface.c_str(),
                  "-s", "/usr/share/udhcpc/default.script", "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
            execl("/bin/busybox", "busybox", "udhcpc", "-i", iface.c_str(),
                  "-s", "/usr/share/udhcpc/default.script", "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
            execlp("udhcpc", "udhcpc", "-i", iface.c_str(),
                   "-s", "/usr/share/udhcpc/default.script", "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
            _exit(0);
        }
        if (dhcp_p > 0) {
            waitpid(dhcp_p, nullptr, 0);
        }

        send_wpa_command("SAVE_CONFIG");

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_connected_ssid = ssid;
            m_status_message = "Connected";
            m_is_connecting.store(false);
        }
        refresh_status_internal();
    } else {
        tinexus::log::warn("[WifiManager] Association with '{}' timed out or failed.", ssid);
        std::lock_guard<std::mutex> lock(m_mutex);
        m_is_connecting.store(false);
        m_status_message = "Connection failed. Verify password.";
    }
}

} // namespace tinexus::settings_ui
