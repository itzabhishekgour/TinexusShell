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
#include <poll.h>
#include <signal.h>

namespace tinexus::settings_ui {

namespace {
std::atomic<uint32_t> g_socket_counter{0};

inline bool safe_exists(const std::string& path_str) noexcept {
    try {
        std::error_code ec;
        return std::filesystem::exists(path_str, ec) && !ec;
    } catch (...) {
        return false;
    }
}

std::string trim_str(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n\"");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n\"");
    return s.substr(start, end - start + 1);
}

std::string run_nmcli_cmd(const std::vector<std::string>& args, int timeout_ms = 5000, int* exit_code_out = nullptr) {
    if (!safe_exists("/usr/bin/nmcli")) {
        if (exit_code_out) *exit_code_out = 127;
        return "";
    }
    int pipefd[2]{-1, -1};
    if (::pipe2(pipefd, O_CLOEXEC) != 0) {
        if (exit_code_out) *exit_code_out = 1;
        return "";
    }
    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        if (exit_code_out) *exit_code_out = 1;
        return "";
    }
    if (pid == 0) {
        ::close(pipefd[0]);
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        int devnull = ::open("/dev/null", O_RDONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDIN_FILENO);
            if (devnull > 2) ::close(devnull);
        }
        if (pipefd[1] > 2) ::close(pipefd[1]);
        std::vector<char*> c_args;
        c_args.reserve(args.size() + 2);
        c_args.push_back(const_cast<char*>("/usr/bin/nmcli"));
        for (const auto& a : args) c_args.push_back(const_cast<char*>(a.c_str()));
        c_args.push_back(nullptr);
        ::execv("/usr/bin/nmcli", c_args.data());
        _exit(127);
    }
    ::close(pipefd[1]);
    int flags = ::fcntl(pipefd[0], F_GETFL, 0);
    if (flags >= 0) ::fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

    std::string out;
    char buf[1024];
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    bool eof = false;
    while (!eof) {
        int rem = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count());
        if (rem <= 0) {
            ::kill(pid, SIGKILL);
            break;
        }
        struct pollfd pfd{};
        pfd.fd = pipefd[0];
        pfd.events = POLLIN;
        int pr = ::poll(&pfd, 1, rem);
        if (pr > 0) {
            while (true) {
                ssize_t n = ::read(pipefd[0], buf, sizeof(buf));
                if (n > 0) out.append(buf, static_cast<size_t>(n));
                else if (n == 0) { eof = true; break; }
                else if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                else if (errno == EINTR) continue;
                else { eof = true; break; }
            }
        } else if (pr == 0) {
            ::kill(pid, SIGKILL);
            break;
        } else if (errno != EINTR) {
            break;
        }
    }
    ::close(pipefd[0]);
    int st = 0;
    while (::waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    if (exit_code_out) {
        *exit_code_out = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
    }
    return out;
}

bool is_nm_running() {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
    int rc = 1;
    std::string out = run_nmcli_cmd({"-t", "-f", "RUNNING", "general"}, 1200, &rc);
#pragma GCC diagnostic pop
    return rc == 0 && out.find("running") != std::string::npos;
}

std::vector<std::string> split_nmcli_escaped(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size() && (line[i + 1] == ':' || line[i + 1] == '\\')) {
            cur.push_back(line[i + 1]);
            ++i;
        } else if (line[i] == ':') {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(line[i]);
        }
    }
    fields.push_back(cur);
    return fields;
}
} // namespace

WifiManager& WifiManager::instance() {
    static WifiManager s_instance;
    return s_instance;
}

WifiManager::WifiManager() {
    try {
        tinexus::log::info("[WifiManager] Initializing native Wi-Fi engine...");
        trigger_scan();
    } catch (const std::exception& e) {
        tinexus::log::error("[WifiManager] Exception during initialization: {}", e.what());
    } catch (...) {
        tinexus::log::error("[WifiManager] Unknown exception during initialization");
    }
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
    try {
        m_wifi_enabled.store(enabled);
        if (is_nm_running()) {
            int rc = 1;
            run_nmcli_cmd({"radio", "wifi", enabled ? "on" : "off"}, 2500, &rc);
            if (!enabled) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_connected_ssid.clear();
                m_ip_address.clear();
                m_networks.clear();
                m_status_message = "Wi-Fi is turned off";
            } else {
                trigger_scan();
            }
            return;
        }
        std::string iface = get_active_interface();
        if (iface.empty()) return;

        if (!enabled) {
            disconnect();
            pid_t p = fork();
            if (p == 0) {
                execl("/usr/bin/sudo", "sudo", "/usr/bin/ip", "link", "set", iface.c_str(), "down", nullptr);
                execl("/usr/bin/sudo", "sudo", "/sbin/ip", "link", "set", iface.c_str(), "down", nullptr);
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
                execl("/usr/bin/sudo", "sudo", "/usr/bin/ip", "link", "set", iface.c_str(), "up", nullptr);
                execl("/usr/bin/sudo", "sudo", "/sbin/ip", "link", "set", iface.c_str(), "up", nullptr);
                execl("/bin/ip", "ip", "link", "set", iface.c_str(), "up", nullptr);
                execl("/sbin/ifconfig", "ifconfig", iface.c_str(), "up", nullptr);
                _exit(0);
            }
            if (p > 0) waitpid(p, nullptr, 0);

            trigger_scan();
        }
    } catch (const std::exception& e) {
        tinexus::log::error("[WifiManager] Error in set_wifi_enabled: {}", e.what());
    } catch (...) {}
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
    if (is_nm_running()) {
        std::string iface = get_active_interface();
        if (!iface.empty()) {
            int rc = 1;
            run_nmcli_cmd({"dev", "disconnect", iface}, 3000, &rc);
        }
    } else {
        send_wpa_command("DISCONNECT");
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connected_ssid.clear();
    m_ip_address.clear();
    m_status_message = "Disconnected";
    for (auto& net : m_networks) {
        net.is_connected = false;
    }
}

bool WifiManager::ensure_wpa_supplicant_running() {
    try {
        std::string iface = get_active_interface();
        if (iface.empty()) {
            return false;
        }

        std::string sock_path = "/var/run/wpa_supplicant/" + iface;
        if (safe_exists(sock_path)) {
            return true;
        }

        // Ensure config directory exists with secure permissions
        try {
            std::error_code ec;
            std::filesystem::create_directories("/etc/wpa_supplicant", ec);
            std::filesystem::create_directories("/var/run/wpa_supplicant", ec);
            chmod("/var/run/wpa_supplicant", 0777);

            if (!safe_exists("/etc/wpa_supplicant/wpa_supplicant.conf")) {
                std::ofstream f("/etc/wpa_supplicant/wpa_supplicant.conf");
                if (f.is_open()) {
                    f << "ctrl_interface=/var/run/wpa_supplicant\nctrl_interface_group=netdev\nupdate_config=1\n";
                }
            }
        } catch (...) {}

        // Bring interface up across all standard tool locations
        pid_t up_p = fork();
        if (up_p == 0) {
            execl("/usr/bin/sudo", "sudo", "/usr/bin/ip", "link", "set", iface.c_str(), "up", nullptr);
            execl("/usr/bin/sudo", "sudo", "/sbin/ip", "link", "set", iface.c_str(), "up", nullptr);
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
            execl("/usr/bin/sudo", "sudo", "/usr/sbin/wpa_supplicant", "-B", "-D", "nl80211,wext", "-i", iface.c_str(),
                  "-c", "/etc/wpa_supplicant/wpa_supplicant.conf", nullptr);
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
            if (safe_exists(sock_path)) {
                return true;
            }
            usleep(100000); // 100ms
        }

        return false;
    } catch (const std::exception& e) {
        tinexus::log::error("[WifiManager] Error in ensure_wpa_supplicant_running: {}", e.what());
        return false;
    } catch (...) {
        return false;
    }
}

std::string WifiManager::send_wpa_command(const std::string& cmd) {
    try {
        std::string iface = get_active_interface();
        if (iface.empty()) return "";

        std::string sock_path = "/var/run/wpa_supplicant/" + iface;
        if (!safe_exists(sock_path)) {
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
    } catch (...) {
        return "";
    }
}

void WifiManager::refresh_status_internal() {
    if (is_nm_running()) {
        int rc = 1;
        std::string raw = run_nmcli_cmd({"-t", "-f", "ACTIVE,SSID,SIGNAL", "dev", "wifi"}, 2000, &rc);
        std::string cur_ssid;
        int bars = 0;
        if (rc == 0) {
            std::istringstream iss(raw);
            std::string line;
            while (std::getline(iss, line)) {
                auto cols = split_nmcli_escaped(line);
                if (cols.size() >= 3 && cols[0] == "yes") {
                    cur_ssid = cols[1];
                    int sig = 50;
                    try { sig = std::clamp(std::stoi(cols[2]), 0, 100); } catch (...) {}
                    bars = (sig >= 75) ? 4 : (sig >= 50) ? 3 : (sig >= 25) ? 2 : 1;
                    break;
                }
            }
        }
        std::string iface = get_active_interface();
        std::string ip;
        if (!iface.empty()) {
            int irc = 1;
            std::string ip_raw = run_nmcli_cmd({"-t", "-f", "IP4.ADDRESS", "dev", "show", iface}, 1500, &irc);
            auto col = ip_raw.find(':');
            if (col != std::string::npos) {
                ip = trim_str(ip_raw.substr(col + 1));
                auto slash = ip.find('/');
                if (slash != std::string::npos) ip = ip.substr(0, slash);
            }
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connected_ssid = cur_ssid;
        m_ip_address = ip;
        m_connected_signal_bars = bars;
        for (auto& net : m_networks) {
            net.is_connected = (!cur_ssid.empty() && net.ssid == cur_ssid);
        }
        m_status_message = !cur_ssid.empty() ? "Connected" : (m_networks.empty() ? "No Wi-Fi networks found" : "Not Connected");
        return;
    }

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
    try {
        int pipefd[2];
        if (pipe(pipefd) < 0) return;

        pid_t pid = fork();
        if (pid == 0) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);

            execl("/usr/bin/sudo", "sudo", "/usr/sbin/iw", "dev", iface.c_str(), "scan", nullptr);
            execl("/usr/bin/sudo", "sudo", "/usr/bin/iw", "dev", iface.c_str(), "scan", nullptr);
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
    } catch (...) {}
}

void WifiManager::scan_worker() {
    try {
        if (is_nm_running()) {
            int rc = 1;
            std::string radio = trim_str(run_nmcli_cmd({"radio", "wifi"}, 1200, &rc));
            if (radio == "disabled") {
                m_wifi_enabled.store(false);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_networks.clear();
                m_status_message = "Wi-Fi is turned off";
                m_is_scanning.store(false);
                return;
            }
            m_wifi_enabled.store(true);

            std::string raw = run_nmcli_cmd({"-t", "-f", "ACTIVE,SSID,BSSID,SIGNAL,SECURITY", "dev", "wifi", "list", "--rescan", "yes"}, 7000, &rc);
            if (rc == 0) {
                std::vector<WifiNetwork> parsed;
                std::string active_ssid;
                int active_bars = 0;
                std::istringstream iss(raw);
                std::string line;
                while (std::getline(iss, line)) {
                    if (line.empty()) continue;
                    auto cols = split_nmcli_escaped(line);
                    if (cols.size() < 5) continue;
                    const bool is_active = (cols[0] == "yes");
                    const std::string ssid = cols[1];
                    if (ssid.empty()) continue;

                    int sig_pct = 50;
                    try { sig_pct = std::clamp(std::stoi(cols[3]), 0, 100); } catch (...) {}
                    int bars = (sig_pct >= 75) ? 4 : (sig_pct >= 50) ? 3 : (sig_pct >= 25) ? 2 : 1;

                    WifiNetwork net;
                    net.ssid = ssid;
                    net.bssid = cols[2];
                    net.signal_dbm = -100 + (sig_pct / 2);
                    net.signal_bars = bars;
                    net.frequency_mhz = 2412;
                    net.security_str = (cols[4].empty() || cols[4] == "--") ? "Open" : cols[4];
                    net.is_secured = (net.security_str != "Open");
                    net.is_connected = is_active;
                    if (is_active) {
                        active_ssid = ssid;
                        active_bars = bars;
                    }
                    auto it = std::find_if(parsed.begin(), parsed.end(), [&](const WifiNetwork& n) { return n.ssid == net.ssid; });
                    if (it == parsed.end()) parsed.push_back(net);
                    else if (net.is_connected || net.signal_bars > it->signal_bars) *it = net;
                }
                std::sort(parsed.begin(), parsed.end(), [](const WifiNetwork& a, const WifiNetwork& b) {
                    if (a.is_connected != b.is_connected) return a.is_connected > b.is_connected;
                    return a.signal_bars > b.signal_bars;
                });

                std::string nm_iface = get_active_interface();
                std::string ip4;
                if (!nm_iface.empty()) {
                    int irc = 1;
                    std::string ip_raw = run_nmcli_cmd({"-t", "-f", "IP4.ADDRESS", "dev", "show", nm_iface}, 1500, &irc);
                    auto col = ip_raw.find(':');
                    if (col != std::string::npos) {
                        ip4 = trim_str(ip_raw.substr(col + 1));
                        auto slash = ip4.find('/');
                        if (slash != std::string::npos) ip4 = ip4.substr(0, slash);
                    }
                }
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_networks = std::move(parsed);
                    m_connected_ssid = active_ssid;
                    m_connected_signal_bars = active_bars;
                    m_ip_address = ip4;
                    if (!nm_iface.empty()) m_active_iface = nm_iface;
                    m_status_message = active_ssid.empty() ? "Disconnected" : "Connected";
                }
                m_is_scanning.store(false);
                return;
            }
        }
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
                execl("/usr/bin/sudo", "sudo", "/usr/bin/ip", "link", "set", hw_res.iface_name.c_str(), "up", nullptr);
                execl("/usr/bin/sudo", "sudo", "/sbin/ip", "link", "set", hw_res.iface_name.c_str(), "up", nullptr);
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
    } catch (const std::exception& e) {
        tinexus::log::error("[WifiManager] Exception in scan_worker: {}", e.what());
    } catch (...) {
        tinexus::log::error("[WifiManager] Unknown exception in scan_worker");
    }
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
    try {
        if (is_nm_running()) {
            int rc = 1;
            std::string iface = get_active_interface();
            std::vector<std::string> args = {"dev", "wifi", "connect", ssid};
            if (!password.empty()) {
                args.push_back("password");
                args.push_back(password);
            }
            if (!iface.empty()) {
                args.push_back("ifname");
                args.push_back(iface);
            }
            std::string res = run_nmcli_cmd(args, 15000, &rc);
            if (rc != 0 && !password.empty()) {
                // Remove stale conflicting connection profile (e.g. old key-mgmt / changed password) and retry cleanly once
                (void)run_nmcli_cmd({"con", "delete", "id", ssid}, 2500, nullptr);
                res = run_nmcli_cmd(args, 15000, &rc);
            }
            if (rc == 0) {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_connected_ssid = ssid;
                    m_connecting_ssid.clear();
                    m_status_message = "Connected";
                }
                m_is_connecting.store(false);
                scan_worker();
                return;
            } else {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_connecting_ssid.clear();
                m_status_message = res.empty() ? "Connection failed. Verify password." : trim_str(res);
                m_is_connecting.store(false);
                return;
            }
        }

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

            // ── Kill any stale dhclient on this interface before acquiring lease ──────
            std::string pid_file   = "/run/dhclient." + iface + ".pid";
            std::string lease_file = "/run/dhclient." + iface + ".leases";
            {
                std::ifstream pf(pid_file);
                if (pf.good()) {
                    pid_t old_pid = 0;
                    pf >> old_pid;
                    if (old_pid > 1) {
                        ::kill(old_pid, SIGTERM);
                        usleep(150000); // 150ms graceful exit
                        ::kill(old_pid, SIGKILL);
                    }
                }
            }
            ::unlink(pid_file.c_str());

            // ── Execute DHCP client ──
            pid_t dhcp_p = fork();
            if (dhcp_p == 0) {
                // Prefer sudo dhclient / dhclient
                execl("/usr/bin/sudo", "sudo", "/sbin/dhclient",
                      "-4", "-v",
                      "-lf", lease_file.c_str(),
                      "-pf", pid_file.c_str(),
                      iface.c_str(), nullptr);
                execl("/usr/bin/sudo", "sudo", "/usr/sbin/dhclient",
                      "-4", "-v",
                      "-lf", lease_file.c_str(),
                      "-pf", pid_file.c_str(),
                      iface.c_str(), nullptr);
                execl("/sbin/dhclient", "dhclient",
                      "-4", "-v",
                      "-lf", lease_file.c_str(),
                      "-pf", pid_file.c_str(),
                      iface.c_str(), nullptr);
                execl("/usr/sbin/dhclient", "dhclient",
                      "-4", "-v",
                      "-lf", lease_file.c_str(),
                      "-pf", pid_file.c_str(),
                      iface.c_str(), nullptr);
                execlp("dhclient", "dhclient",
                       "-4", "-v",
                       "-lf", lease_file.c_str(),
                       "-pf", pid_file.c_str(),
                       iface.c_str(), nullptr);

                // busybox / udhcpc fallback
                execl("/usr/bin/udhcpc", "udhcpc", "-i", iface.c_str(),
                      "-s", "/usr/share/udhcpc/default.script",
                      "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
                execl("/bin/udhcpc", "udhcpc", "-i", iface.c_str(),
                      "-s", "/usr/share/udhcpc/default.script",
                      "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
                execl("/sbin/udhcpc", "udhcpc", "-i", iface.c_str(),
                      "-s", "/usr/share/udhcpc/default.script",
                      "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
                execl("/bin/busybox", "busybox", "udhcpc", "-i", iface.c_str(),
                      "-s", "/usr/share/udhcpc/default.script",
                      "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
                execlp("udhcpc", "udhcpc", "-i", iface.c_str(),
                       "-s", "/usr/share/udhcpc/default.script",
                       "-x", "hostname:Tinexus-Desktop", "-q", "-n", nullptr);
                _exit(127);
            }
            if (dhcp_p > 0) {
                int wstatus = 0;
                waitpid(dhcp_p, &wstatus, 0);
                if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 127) {
                    tinexus::log::warn("[WifiManager] No DHCP client binary found — lease not acquired for '{}'.", iface);
                }
            }

            send_wpa_command("SAVE_CONFIG");

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_connected_ssid = ssid;
                m_status_message = "Connected";
                m_is_connecting.store(false);
            }

            // Poll for IPv4 lease acquisition for up to 10 seconds
            for (int ip_wait = 0; ip_wait < 20; ++ip_wait) {
                refresh_status_internal();
                if (!get_ip_address().empty()) {
                    tinexus::log::info("[WifiManager] Acquired IPv4 lease: {}", get_ip_address());
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            if (get_ip_address().empty()) {
                tinexus::log::warn("[WifiManager] DHCP lease not acquired within 10s for interface '{}'.", iface);
            }
        } else {
            tinexus::log::warn("[WifiManager] Association with '{}' timed out or failed.", ssid);
            std::lock_guard<std::mutex> lock(m_mutex);
            m_is_connecting.store(false);
            m_status_message = "Connection failed. Verify password.";
        }
    } catch (const std::exception& e) {
        tinexus::log::error("[WifiManager] Exception in connect_worker: {}", e.what());
    } catch (...) {
        tinexus::log::error("[WifiManager] Unknown exception in connect_worker");
    }
    m_is_connecting.store(false);
}

} // namespace tinexus::settings_ui
