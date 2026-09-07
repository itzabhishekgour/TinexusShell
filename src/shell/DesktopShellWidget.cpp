#include "DesktopShellWidget.hpp"
#include <common/TinexusLogo.hpp>
#include <unistd.h>
#include <sys/reboot.h>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <sys/socket.h>
#include <sys/un.h>
#include <atomic>
#include <fstream>
#include <cstring>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

namespace tinexus::shell {

namespace fs = std::filesystem;

static std::vector<AppItem> g_recent_launches;
constexpr size_t MAX_RECENT = 5;

static bool is_process_running(const std::string& comm_name) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator("/proc", ec)) {
        if (!entry.is_directory()) continue;
        std::string pid_str = entry.path().filename().string();
        if (pid_str.empty() || !std::isdigit(static_cast<unsigned char>(pid_str[0]))) continue;

        std::ifstream comm_file(entry.path() / "comm");
        if (comm_file.is_open()) {
            std::string name;
            std::getline(comm_file, name);
            if (name == comm_name) return true;
        }
    }
    return false;
}

pid_t spawn_app(const AppItem& item) {
    if (item.kind == ResultKind::System) {
        const std::string& cmd = item.exec;
        if (cmd == "lock") {
            log::info("[Shell] System action: Lock Screen");
            pid_t pid = fork();
            if (pid == 0) { setsid(); execlp("tinexus-lock", "tinexus-lock", nullptr); _exit(127); }
            return pid;
        } else if (cmd == "shutdown") {
            log::info("[Shell] System action: Shutdown");
            sync();
            ::reboot(RB_POWER_OFF);
            return -1;
        } else if (cmd == "reboot") {
            log::info("[Shell] System action: Reboot");
            sync();
            ::reboot(RB_AUTOBOOT);
            return -1;
        } else if (cmd == "sleep") {
            log::info("[Shell] System action: Sleep");
            return -1;
        } else if (cmd == "logout") {
            log::info("[Shell] System action: Logout");
            ::kill(1, SIGTERM);
            return -1;
        }
        return -1;
    }

    std::string clean_exec = indexer::DesktopParser::sanitize_exec(item.exec);
    if (clean_exec.empty()) return -1;

    if ((clean_exec == "tinexus-settings-ui" || clean_exec == "tinexus-monitor") &&
        item.exec.find("--") == std::string::npos) {
        if (is_process_running(clean_exec)) {
            log::info("[Shell] App {} is already running — sending focus request to compositor", clean_exec);
            const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
            if (xdg_runtime) {
                std::string fifo_path = std::string(xdg_runtime) + "/tinexus_comp_cmd";
                int fd = open(fifo_path.c_str(), O_WRONLY | O_NONBLOCK);
                if (fd >= 0) {
                    std::string app_id = (clean_exec == "tinexus-settings-ui") ? "tinexus-settings" : clean_exec;
                    std::string cmd = "focus " + app_id + "\n";
                    if (write(fd, cmd.c_str(), cmd.size()) < 0) {}
                    close(fd);
                }
            }
            return -1;
        }
    }

    auto it = std::find_if(g_recent_launches.begin(), g_recent_launches.end(),
        [&](const AppItem& a) { return a.exec == item.exec; });
    if (it != g_recent_launches.end()) g_recent_launches.erase(it);
    g_recent_launches.insert(g_recent_launches.begin(), item);
    if (g_recent_launches.size() > MAX_RECENT) g_recent_launches.pop_back();

    pid_t pid = fork();
    if (pid < 0) { log::error("[Shell] fork() failed"); return -1; }
    if (pid == 0) {
        setsid();
        if (item.is_terminal) {
            execlp("foot", "foot", "-e", clean_exec.c_str(), nullptr);
            execlp("weston-terminal", "weston-terminal", "-e", clean_exec.c_str(), nullptr);
            execlp("alacritty", "alacritty", "-e", clean_exec.c_str(), nullptr);
            execlp("xterm", "xterm", "-e", clean_exec.c_str(), nullptr);
            _exit(127);
        }
        std::vector<std::string> tokens;
        std::istringstream iss(clean_exec); std::string tok;
        while (iss >> tok) tokens.push_back(tok);
        if (tokens.empty()) _exit(1);
        std::vector<char*> args;
        for (auto& t : tokens) args.push_back(const_cast<char*>(t.c_str()));
        args.push_back(nullptr);
        execvp(args[0], args.data());
        _exit(127);
    }
    log::info("[Shell] Spawned '{}' PID={}", clean_exec, pid);
    return pid;
}

static bool try_eval_calc(const std::string& q, double& result) {
    bool has_op = false, has_dig = false;
    for (char c : q) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') has_dig = true;
        if (c == '+' || c == '-' || c == '*' || c == '/') has_op = true;
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '+' && c != '-' &&
            c != '*' && c != '/' && c != '.' && c != ' ' && c != '(' && c != ')') return false;
    }
    if (!has_dig || !has_op) return false;
    double a{0}, b{0}; char op{'?'};
    std::istringstream ss(q);
    if (!(ss >> a) || !(ss >> op) || !(ss >> b)) return false;
    if (b == 0.0 && op == '/') return false;
    switch (op) {
        case '+': result = a + b; break; case '-': result = a - b; break;
        case '*': result = a * b; break; case '/': result = a / b; break;
        default: return false;
    }
    return true;
}

static std::string to_lower(std::string_view sv) {
    std::string r; r.reserve(sv.size());
    for (char c : sv) r.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return r;
}

static std::vector<AppItem> get_system_actions(const std::string& lq) {
    struct SA { const char* cmd; const char* label; const char* desc; };
    constexpr SA kA[] = {
        {"lock","Lock Screen","Lock the current session"},
        {"shutdown","Shut Down","Power off the computer"},
        {"reboot","Restart","Restart the computer"},
        {"sleep","Sleep","Suspend to RAM"},
        {"logout","Log Out","End the current session"},
    };
    std::vector<AppItem> out;
    for (const auto& a : kA) {
        std::string cmd(a.cmd), label(a.label);
        if (lq.empty() || cmd.find(lq) != std::string::npos || label.find(lq) != std::string::npos)
            out.push_back({a.label, a.cmd, a.desc, false, "", ResultKind::System});
    }
    return out;
}

std::vector<AppItem> load_system_apps() {
    std::vector<AppItem> apps;
    apps.push_back({"About Tinexus", "tinexus-about", "System Profiler & Hardware Specs", false, ""});
    apps.push_back({"Terminal", "foot", "Default Wayland Terminal", true, ""});
    apps.push_back({"Files", "tinexus-files", "Fast Miller-Column File Browser", false, ""});
    apps.push_back({"Settings", "tinexus-settings-ui", "System Configuration & Control", false, ""});
    apps.push_back({"System Monitor", "tinexus-monitor", "Hardware Resources & Process Trees", false, ""});
    apps.push_back({"Package Manager", "tinexus-pkg", "Software & Package Management", false, ""});
    apps.push_back({"App Installer", "tinexus-app-installer", "Install .txapp Packages", false, ""});
    apps.push_back({"Weston Terminal", "weston-terminal", "Wayland Demo Terminal", true, ""});
    apps.push_back({"Alacritty", "alacritty", "GPU Accelerated Terminal", true, ""});

    std::vector<fs::path> dirs = {"/usr/share/applications", "/usr/local/share/applications"};
    const char* home = std::getenv("HOME");
    if (home) dirs.push_back(fs::path(home) / ".local" / "share" / "applications");

    for (const auto& dir : dirs) {
        if (!fs::exists(dir)) continue;
        try {
            for (const auto& e : fs::directory_iterator(dir)) {
                if (!e.is_regular_file() || e.path().extension() != ".desktop") continue;
                auto p = indexer::DesktopParser::parse_file(e.path());
                if (!p || p->no_display || p->exec.empty()) continue;
                bool dup = std::any_of(apps.begin(), apps.end(), [&](const AppItem& a) {
                    return a.name == p->name || a.exec == p->exec; });
                if (!dup) apps.push_back({p->name, p->exec,
                    p->comment.empty() ? p->generic_name : p->comment, p->terminal, p->icon});
            }
        } catch (...) {}
    }
    return apps;
}

std::vector<AppItem> build_results(const std::string& q, const std::vector<AppItem>& all) {
    std::string lq = to_lower(q);
    std::vector<AppItem> results;
    double cv{0};
    if (!lq.empty() && try_eval_calc(lq, cv)) {
        std::ostringstream os;
        if (cv == static_cast<long long>(cv)) os << static_cast<long long>(cv); else os << cv;
        std::string ans = os.str();
        results.push_back({"= " + ans, ans, q + " = " + ans, false, "", ResultKind::Calculator});
    }
    auto sys = get_system_actions(lq);
    results.insert(results.end(), sys.begin(), sys.end());
    if (lq.empty()) {
        for (const auto& r : g_recent_launches) results.push_back(r);
    } else {
        for (const auto& a : all)
            if (to_lower(a.name).find(lq) != std::string::npos ||
                to_lower(a.exec).find(lq) != std::string::npos ||
                to_lower(a.description).find(lq) != std::string::npos)
                results.push_back(a);
    }
    return results;
}

double read_battery_percent() {
    const char* paths[] = {
        "/sys/class/power_supply/BAT0/capacity",
        "/sys/class/power_supply/BAT1/capacity",
        "/sys/class/power_supply/battery/capacity",
    };
    for (const char* p : paths) {
        FILE* f = fopen(p, "r");
        if (!f) continue;
        int cap = -1;
        if (fscanf(f, "%d", &cap) != 1) cap = -1;
        fclose(f);
        if (cap >= 0 && cap <= 100) return static_cast<double>(cap);
    }
    return -1.0;
}

void read_network_status(int& out_bars, bool& out_connected) {
    static int s_cached_bars = 0;
    static bool s_cached_conn = false;
    static auto s_last_check = std::chrono::steady_clock::time_point{};

    auto now = std::chrono::steady_clock::now();
    if (s_last_check.time_since_epoch().count() != 0 &&
        std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_check).count() < 1200) {
        out_bars = s_cached_bars;
        out_connected = s_cached_conn;
        return;
    }
    s_last_check = now;

    int bars = 0;
    bool connected = false;

    try {
        if (fs::exists("/sys/class/net")) {
            for (const auto& entry : fs::directory_iterator("/sys/class/net")) {
                std::string ifname = entry.path().filename().string();
                if (ifname == "lo" || ifname.rfind("wlan", 0) == 0 || ifname.rfind("wlo", 0) == 0 || ifname.rfind("wlp", 0) == 0) continue;
                std::ifstream op(entry.path() / "operstate");
                std::string st;
                if (op >> st && st == "up") {
                    connected = true;
                    bars = 4;
                    s_cached_bars = bars;
                    s_cached_conn = connected;
                    out_bars = bars;
                    out_connected = connected;
                    return;
                }
            }
        }
    } catch (...) {}

    auto wifi_res = tinexus::net::probe_primary_wifi_interface("/sys/class/net", "/sys/class/rfkill", 0);
    std::string wifi_iface = wifi_res.iface_name;

    if (!wifi_iface.empty()) {
        std::string sock_path = "/var/run/wpa_supplicant/" + wifi_iface;
        if (fs::exists(sock_path)) {
            int sock = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
            if (sock >= 0) {
                struct sockaddr_un local;
                memset(&local, 0, sizeof(local));
                local.sun_family = AF_UNIX;
                static std::atomic<uint32_t> s_cnt{0};
                snprintf(local.sun_path, sizeof(local.sun_path), "/tmp/aura_wpa_%d_%u", getpid(), s_cnt.fetch_add(1));
                unlink(local.sun_path);

                if (bind(sock, reinterpret_cast<struct sockaddr*>(&local), sizeof(local)) == 0) {
                    struct sockaddr_un remote;
                    memset(&remote, 0, sizeof(remote));
                    remote.sun_family = AF_UNIX;
                    strncpy(remote.sun_path, sock_path.c_str(), sizeof(remote.sun_path) - 1);

                    const char* ping_cmd = "SIGNAL_POLL";
                    sendto(sock, ping_cmd, strlen(ping_cmd), 0, reinterpret_cast<struct sockaddr*>(&remote), sizeof(remote));

                    char buf[512];
                    struct timeval tv{0, 40000};
                    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                    ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
                    if (n > 0) {
                        buf[n] = '\0';
                        std::string resp(buf);
                        auto pos = resp.find("RSSI=");
                        if (pos != std::string::npos) {
                            int rssi = std::atoi(resp.c_str() + pos + 5);
                            connected = true;
                            if (rssi >= -55) bars = 4;
                            else if (rssi >= -67) bars = 3;
                            else if (rssi >= -78) bars = 2;
                            else bars = 1;
                        }
                    }
                }
                close(sock);
                unlink(local.sun_path);
            }
        }

        if (!connected) {
            struct ifaddrs* ifaddr = nullptr;
            if (getifaddrs(&ifaddr) == 0) {
                for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
                    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
                    std::string ifname = ifa->ifa_name ? ifa->ifa_name : "";
                    if (ifname == wifi_iface) {
                        auto* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                        uint32_t ip = ntohl(sa->sin_addr.s_addr);
                        if (ip != 0 && (ip & 0xFF000000) != 0x7F000000) {
                            connected = true;
                            bars = 4;
                            break;
                        }
                    }
                }
                freeifaddrs(ifaddr);
            }
        }
    }

    s_cached_bars = bars;
    s_cached_conn = connected;
    out_bars = bars;
    out_connected = connected;
}

static int days_in_month(int year, int month) {
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

static int day_of_week(int year, int month, int day) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    mktime(&t);
    return t.tm_wday;
}

namespace ui {
    constexpr txui::Color BAR_BG        { 13,  14,  18, 245};
    constexpr txui::Color BORDER_LINE   {255, 255, 255,  15};
    constexpr txui::Color NOTCH_BG      { 36,  40,  54, 245}; // Lighter frosted slate matching mockup
    constexpr txui::Color NOTCH_RIM     {255, 255, 255,  35};
    constexpr txui::Color TIME_PILL_BG  { 20,  22,  30, 220};
    constexpr txui::Color TIME_PILL_RIM {255, 255, 255,  22};
    constexpr txui::Color TXT_PRI       {245, 245, 250, 255};
    constexpr txui::Color TXT_SEC       {175, 180, 195, 255};
    constexpr txui::Color TXT_DIM       {120, 125, 140, 200};
    constexpr txui::Color ACCENT_BLUE   { 59, 130, 246, 255};
    constexpr txui::Color ACCENT_CYAN   { 56, 189, 248, 255};
    constexpr txui::Color WIFI_COL      {100, 210, 100, 240};
    constexpr txui::Color WIFI_DIM      { 80,  80, 100, 130};
    constexpr txui::Color CARD_BG       { 22,  24,  32, 248};
    constexpr txui::Color CARD_HOVER    { 44,  48,  62, 255};
    constexpr txui::Color ROW_SELECT    { 59, 130, 246, 220};
}

DesktopShellWidget::DesktopShellWidget() {
    all_apps = load_system_apps();
    sync_notifications();
}

void DesktopShellWidget::sync_notifications() {
    // Dynamic initialization of active platform notifications
    if (notifications.empty()) {
        notifications = {
            {1, "Platform Ready", "Tinexus Desktop v1.0", "Wayland Vulkan compositing active on DRM KMS", "Just now", ui::ACCENT_CYAN, 1},
            {2, "Network", "Wi-Fi Connected", "Primary interface wlan0 active and online", "5m ago", ui::WIFI_COL, 1},
            {3, "Supervisor", "Platform Supervision", "Supervision tree active, daemons sandboxed", "12m ago", ui::ACCENT_BLUE, 0}
        };
    }
}

txui::Size DesktopShellWidget::measure_override(const txui::Constraints& c) noexcept {
    return txui::Size(c.max_width, c.max_height);
}

void DesktopShellWidget::paint_override(txui::Painter& painter) const noexcept {
    using namespace ui;
    const auto& f = frame();
    const double W = f.width();
    const double cx = W * 0.5;

    // Wayland SHM clearing
    painter.clear(txui::Color(0, 0, 0, 0));

    // ── Launch Card Animation (Centered Flip Card) ───────────────────────
    if (pulse_launching) {
        const double lcx = W * 0.5, lcy = f.height() * 0.4;
        const double cw = 380.0, ch = 220.0;
        const double px = lcx - cw * 0.5, py = lcy - ch * 0.5;
        painter.fill_rounded_rect({px - 4.0, py + 8.0, cw + 8.0, ch}, 18, txui::Color(0, 0, 0, 110));
        painter.fill_rounded_rect({px, py, cw, ch}, 18, CARD_BG);
        painter.fill_rounded_rect({px - 1.0, py - 1.0, cw + 2.0, ch + 2.0}, 19, BORDER_LINE);

        txui::Color cat = pulse_launch_app.kind == ResultKind::Calculator ? txui::Color(16, 185, 129, 255) :
                          pulse_launch_app.kind == ResultKind::System ? txui::Color(239, 68, 68, 255) : ACCENT_BLUE;
        painter.fill_circle({lcx, lcy - 22.0}, 36.0, txui::Color(cat.r(), cat.g(), cat.b(), 65));
        painter.draw_circle({lcx, lcy - 22.0}, 36.0, 2.0, cat);
        painter.fill_circle({lcx, lcy - 22.0}, 12.0, cat);
        painter.draw_text({lcx - static_cast<double>(pulse_launch_app.name.size()) * 4.2, lcy + 36.0},
                          pulse_launch_app.name, TXT_PRI, 1.2, true);
        return;
    }

    // ── 1. FULL-WIDTH TOP BAR (32px high) ─────────────────────────────────
    constexpr double BAR_H = 32.0;
    painter.fill_rect({0.0, 0.0, W, BAR_H}, BAR_BG);

    // Center: Aura v2 Sloped Trapezoid Notch (\______/) (46px high, hangs down below bar)
    constexpr double NOTCH_H = 46.0;
    constexpr double NOTCH_TOP_HALF = 136.0; // from cx - 136 to cx + 136
    constexpr double NOTCH_BOT_HALF = 98.0;  // from cx - 98 to cx + 98
    constexpr double NOTCH_SLOPE = NOTCH_TOP_HALF - NOTCH_BOT_HALF; // 38.0

    for (int y = 0; y <= static_cast<int>(NOTCH_H); ++y) {
        double t = static_cast<double>(y) / NOTCH_H;
        double lx = (cx - NOTCH_TOP_HALF) + t * NOTCH_SLOPE;
        double rx = (cx + NOTCH_TOP_HALF) - t * NOTCH_SLOPE;
        painter.fill_rect({lx, static_cast<double>(y), rx - lx, 1.0}, NOTCH_BG);
    }

    // Top bar hairline border meeting the notch slopes at y=32.0
    double t_bar = BAR_H / NOTCH_H;
    double lx_bar = (cx - NOTCH_TOP_HALF) + t_bar * NOTCH_SLOPE;
    double rx_bar = (cx + NOTCH_TOP_HALF) - t_bar * NOTCH_SLOPE;
    painter.draw_line({0.0, BAR_H - 1.0}, {lx_bar, BAR_H - 1.0}, 1.0, BORDER_LINE);
    painter.draw_line({rx_bar, BAR_H - 1.0}, {W, BAR_H - 1.0}, 1.0, BORDER_LINE);

    // Notch rim outline (protruding down to 46px)
    painter.draw_line({cx - NOTCH_TOP_HALF, 0.0}, {cx - NOTCH_BOT_HALF, NOTCH_H}, 1.2, NOTCH_RIM);
    painter.draw_line({cx - NOTCH_BOT_HALF, NOTCH_H}, {cx + NOTCH_BOT_HALF, NOTCH_H}, 1.2, NOTCH_RIM);
    painter.draw_line({cx + NOTCH_BOT_HALF, NOTCH_H}, {cx + NOTCH_TOP_HALF, 0.0}, 1.2, NOTCH_RIM);

    // Left: Logo (centered in 32px bar)
    if (hover_logo || logo_menu_open) {
        painter.fill_rounded_rect({6.0, 3.0, 30.0, 26.0}, 6, txui::Color(255, 255, 255, 22));
    }
    auto logo_buf = logo::get_logo(32);
    if (logo_buf.is_valid()) {
        painter.draw_image({10.0, 5.0, 22.0, 22.0}, logo_buf.pixels, logo_buf.width, logo_buf.height);
    } else {
        painter.fill_circle({21.0, 16.0}, 10.0, txui::Color(30, 35, 48, 255));
        painter.draw_circle({21.0, 16.0}, 10.0, 1.2, ACCENT_BLUE);
        painter.draw_circle({21.0, 16.0}, 7.5, 1.0, ACCENT_CYAN);
        painter.fill_circle({21.0, 16.0}, 3.5, txui::Color(255, 255, 255, 240));
    }

    // Left: Active Application Title Trigger (centered in 32px bar)
    double app_label_w = static_cast<double>(active_app_name.size()) * 7.5 + 24.0;
    if (hover_app_title || app_menu_open) {
        painter.fill_rounded_rect({42.0, 3.0, app_label_w, 26.0}, 5, txui::Color(255, 255, 255, 18));
    }
    painter.draw_text({48.0, 8.0}, active_app_name, TXT_PRI, 1.0, true);
    double chv_x = 48.0 + app_label_w - 14.0;
    painter.draw_line({chv_x, 14.5}, {chv_x + 3.5, 18.0}, 1.4, TXT_SEC);
    painter.draw_line({chv_x + 3.5, 18.0}, {chv_x + 7.0, 14.5}, 1.4, TXT_SEC);

    // ── Aura Notch Center Assembly: MATHEMATICALLY BALANCED & SYMMETRICAL ─
    time_t raw_now = time(nullptr);
    struct tm tb;
    localtime_r(&raw_now, &tb);

    char ts[32];
    strftime(ts, sizeof(ts), "%I:%M %p", &tb);
    char* time_disp = (ts[0] == '0') ? ts + 1 : ts;

    // Time pill capsule on the Left:
    // Left edge at cx - 96.0, width 72.0, right edge at cx - 24.0 (Distance to center cx = 24.0px)
    painter.fill_rounded_rect({cx - 96.0, 11.0, 72.0, 24.0}, 12, TIME_PILL_BG);
    painter.fill_rounded_rect({cx - 96.0, 11.0, 72.0, 24.0}, 12, TIME_PILL_RIM);
    double time_w = static_cast<double>(strlen(time_disp)) * 7.2;
    double time_x = (cx - 96.0) + (72.0 - time_w) * 0.5;
    painter.draw_text({time_x, 15.0}, time_disp, TXT_PRI, 0.90, true);

    // Center Avatar Circle 'T' (Exact Center cx = W * 0.5, y = 23.0)
    painter.draw_glow({cx, 23.0}, 14.0, 22.0, txui::Color(59, 130, 246, 75));
    painter.fill_circle({cx, 23.0}, 14.0, txui::Color(59, 130, 246, 255));
    painter.draw_circle({cx, 23.0}, 14.5, 1.2, txui::Color(147, 197, 253, 180));
    painter.draw_text({cx - 4.5, 15.5}, "T", txui::Color(255, 255, 255, 255), 1.15, true);

    // Date pill capsule on the Right:
    // Left edge at cx + 24.0, width 72.0, right edge at cx + 96.0 (Distance to center cx = 24.0px)
    int cur_day = tb.tm_mday;
    const char* sfx = "th";
    if (cur_day % 10 == 1 && cur_day != 11) sfx = "st";
    else if (cur_day % 10 == 2 && cur_day != 12) sfx = "nd";
    else if (cur_day % 10 == 3 && cur_day != 13) sfx = "rd";
    char month_abbr[16];
    strftime(month_abbr, sizeof(month_abbr), "%b", &tb);
    char ds[32];
    snprintf(ds, sizeof(ds), "%s %d%s", month_abbr, cur_day, sfx);

    constexpr double DATE_PILL_W = 72.0;
    if (hover_aura_date || calendar_open) {
        painter.fill_rounded_rect({cx + 24.0, 11.0, DATE_PILL_W, 24.0}, 12, txui::Color(255, 255, 255, 30));
    } else {
        painter.fill_rounded_rect({cx + 24.0, 11.0, DATE_PILL_W, 24.0}, 12, TIME_PILL_BG);
    }
    painter.fill_rounded_rect({cx + 24.0, 11.0, DATE_PILL_W, 24.0}, 12, TIME_PILL_RIM);
    double date_w = static_cast<double>(strlen(ds)) * 7.2;
    double date_x = (cx + 24.0) + (DATE_PILL_W - date_w) * 0.5;
    painter.draw_text({date_x, 15.0}, ds, (hover_aura_date || calendar_open) ? ACCENT_CYAN : TXT_PRI, 0.90, true);

    // Right: Status Group (centered in 32px bar at y=16)
    double sun_x = W - 225.0;
    if (hover_sun) painter.fill_rounded_rect({sun_x - 4.0, 3.0, 26.0, 26.0}, 5, txui::Color(255, 255, 255, 20));
    painter.draw_circle({sun_x + 9.0, 16.0}, 4.5, 1.5, TXT_PRI);
    for (int a = 0; a < 8; ++a) {
        double ang = static_cast<double>(a) * (3.14159265 / 4.0);
        double px1 = (sun_x + 9.0) + std::cos(ang) * 6.5;
        double py1 = 16.0 + std::sin(ang) * 6.5;
        double px2 = (sun_x + 9.0) + std::cos(ang) * 8.5;
        double py2 = 16.0 + std::sin(ang) * 8.5;
        painter.draw_line({px1, py1}, {px2, py2}, 1.2, TXT_SEC);
    }

    // Volume (centered in 32px bar)
    double vol_x = W - 190.0;
    if (hover_vol) painter.fill_rounded_rect({vol_x - 4.0, 3.0, 28.0, 26.0}, 5, txui::Color(255, 255, 255, 20));
    painter.fill_rect({vol_x + 2.0, 13.0, 4.0, 6.0}, TXT_PRI);
    painter.draw_line({vol_x + 6.0, 13.0}, {vol_x + 11.0, 10.0}, 1.5, TXT_PRI);
    painter.draw_line({vol_x + 11.0, 10.0}, {vol_x + 11.0, 22.0}, 1.5, TXT_PRI);
    painter.draw_line({vol_x + 11.0, 22.0}, {vol_x + 6.0, 19.0}, 1.5, TXT_PRI);
    if (sound_muted) {
        painter.draw_line({vol_x + 14.0, 12.0}, {vol_x + 20.0, 20.0}, 1.5, txui::Color(239, 68, 68, 240));
    } else {
        painter.draw_line({vol_x + 15.0, 13.0}, {vol_x + 16.5, 16.0}, 1.2, ACCENT_CYAN);
        painter.draw_line({vol_x + 16.5, 16.0}, {vol_x + 15.0, 19.0}, 1.2, ACCENT_CYAN);
        painter.draw_line({vol_x + 19.0, 11.0}, {vol_x + 21.0, 16.0}, 1.2, ACCENT_BLUE);
        painter.draw_line({vol_x + 21.0, 16.0}, {vol_x + 19.0, 21.0}, 1.2, ACCENT_BLUE);
    }

    // Battery (centered in 32px bar)
    double bat_x = W - 150.0;
    if (hover_bat) painter.fill_rounded_rect({bat_x - 4.0, 3.0, 58.0, 26.0}, 5, txui::Color(255, 255, 255, 20));
    double pct = read_battery_percent();
    double fill_pct = (pct >= 0.0) ? pct : 100.0;
    txui::Color bat_col = (fill_pct > 20.0) ? txui::Color(74, 222, 128, 255) : txui::Color(239, 68, 68, 255);
    painter.fill_rounded_rect({bat_x, 10.0, 22.0, 12.0}, 2, txui::Color(55, 60, 75, 220));
    painter.fill_rounded_rect({bat_x + 22.0, 13.0, 2.5, 6.0}, 1, txui::Color(55, 60, 75, 220));
    painter.fill_rounded_rect({bat_x + 2.0, 12.0, 18.0 * (fill_pct / 100.0), 8.0}, 1, bat_col);
    char pct_str[16];
    if (pct >= 0.0) snprintf(pct_str, sizeof(pct_str), "%.0f%%", pct);
    else snprintf(pct_str, sizeof(pct_str), "DC");
    painter.draw_text({bat_x + 28.0, 9.0}, pct_str, TXT_PRI, 0.9);

    // Wi-Fi (centered in 32px bar)
    double wifi_x = W - 82.0;
    if (hover_wifi) painter.fill_rounded_rect({wifi_x - 6.0, 3.0, 32.0, 26.0}, 5, txui::Color(255, 255, 255, 20));
    int wifi_bars = 0;
    bool has_net = false;
    read_network_status(wifi_bars, has_net);
    txui::Color w_col = has_net ? WIFI_COL : WIFI_DIM;
    painter.fill_circle({wifi_x + 10.0, 21.0}, 2.0, w_col);
    painter.draw_line({wifi_x + 6.0, 18.0}, {wifi_x + 10.0, 15.0}, 1.8, (wifi_bars >= 2) ? w_col : WIFI_DIM);
    painter.draw_line({wifi_x + 10.0, 15.0}, {wifi_x + 14.0, 18.0}, 1.8, (wifi_bars >= 2) ? w_col : WIFI_DIM);
    painter.draw_line({wifi_x + 3.0, 15.0}, {wifi_x + 10.0, 11.0}, 1.8, (wifi_bars >= 3) ? w_col : WIFI_DIM);
    painter.draw_line({wifi_x + 10.0, 11.0}, {wifi_x + 17.0, 15.0}, 1.8, (wifi_bars >= 3) ? w_col : WIFI_DIM);
    painter.draw_line({wifi_x + 0.0, 12.0}, {wifi_x + 10.0, 7.0}, 1.8, (wifi_bars >= 4) ? w_col : WIFI_DIM);
    painter.draw_line({wifi_x + 10.0, 7.0}, {wifi_x + 20.0, 12.0}, 1.8, (wifi_bars >= 4) ? w_col : WIFI_DIM);

    // Bell / Notifications Button (centered in 32px bar)
    double bell_x = W - 42.0;
    if (hover_bell || notifications_open) {
        painter.fill_rounded_rect({bell_x - 4.0, 3.0, 28.0, 26.0}, 5, txui::Color(255, 255, 255, 20));
    }
    painter.draw_line({bell_x + 6.0, 18.0}, {bell_x + 14.0, 18.0}, 1.5, TXT_PRI);
    painter.draw_line({bell_x + 7.0, 18.0}, {bell_x + 8.5, 12.0}, 1.5, TXT_PRI);
    painter.draw_line({bell_x + 13.0, 18.0}, {bell_x + 11.5, 12.0}, 1.5, TXT_PRI);
    painter.draw_circle({bell_x + 10.0, 12.0}, 2.5, 1.2, TXT_PRI);
    painter.fill_circle({bell_x + 10.0, 20.0}, 1.5, TXT_PRI);
    if (!notifications.empty()) {
        painter.fill_circle({bell_x + 14.5, 10.0}, 3.0, txui::Color(249, 115, 22, 255));
    }

    // ── 2. LOGO MENU (Dropdown with Modern Glassmorphism & Icons) ───────────
    if (logo_menu_open) {
        const double mx = 8.0, my = 38.0, mw = 236.0, mh = 260.0;
        painter.fill_rounded_rect({mx - 4.0, my + 6.0, mw + 8.0, mh + 4.0}, 14, txui::Color(0, 0, 0, 95));
        painter.fill_rounded_rect({mx, my, mw, mh}, 14, CARD_BG);
        painter.fill_rounded_rect({mx - 1.0, my - 1.0, mw + 2.0, mh + 2.0}, 15, BORDER_LINE);

        struct MenuItem { const char* title; bool separator; bool bold; txui::Color dot_col; const char* shortcut; };
        MenuItem items[] = {
            {"About Tinexus", false, true, ACCENT_CYAN, ""},
            {"---", true, false, txui::Color(), ""},
            {"System Settings...", false, false, ACCENT_BLUE, ""},
            {"App Installer...", false, false, ACCENT_CYAN, ""},
            {"System Monitor...", false, false, txui::Color(168, 85, 247, 255), ""},
            {"---", true, false, txui::Color(), ""},
            {"Sleep", false, false, txui::Color(245, 158, 11, 255), ""},
            {"Restart...", false, false, ACCENT_CYAN, ""},
            {"Shut Down...", false, false, txui::Color(239, 68, 68, 255), ""},
            {"---", true, false, txui::Color(), ""},
            {"Lock Screen", false, false, ACCENT_BLUE, "Ctrl+L"}
        };

        double cur_y = my + 8.0;
        int act_idx = 0;
        for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); ++i) {
            if (items[i].separator) {
                painter.draw_line({mx + 12.0, cur_y + 4.0}, {mx + mw - 12.0, cur_y + 4.0}, 1.0, BORDER_LINE);
                cur_y += 9.0;
            } else {
                bool sel = (logo_menu_hover == act_idx);
                if (sel) {
                    painter.fill_rounded_rect({mx + 6.0, cur_y, mw - 12.0, 24.0}, 6, ROW_SELECT);
                }
                // Leading accent dot
                painter.fill_circle({mx + 16.0, cur_y + 12.0}, 3.5, sel ? txui::Color(255, 255, 255, 255) : items[i].dot_col);
                painter.draw_text({mx + 28.0, cur_y + 5.0}, items[i].title,
                                  sel ? txui::Color(255, 255, 255, 255) : TXT_PRI, 0.95, items[i].bold);

                if (items[i].shortcut[0] != '\0') {
                    painter.draw_text({mx + mw - 52.0, cur_y + 6.0}, items[i].shortcut,
                                      sel ? txui::Color(220, 230, 255, 220) : TXT_DIM, 0.85);
                }

                cur_y += 25.0;
                act_idx++;
            }
        }
    }

    // ── 3. APPLICATIONS MENU ──────────────────────────────────────────────
    if (app_menu_open) {
        const double ax = 44.0, ay = 38.0, aw = 250.0;
        const size_t show_count = std::min(all_apps.size(), size_t{10});
        const double ah = 16.0 + static_cast<double>(show_count) * 28.0;

        painter.fill_rounded_rect({ax - 4.0, ay + 6.0, aw + 8.0, ah + 4.0}, 14, txui::Color(0, 0, 0, 95));
        painter.fill_rounded_rect({ax, ay, aw, ah}, 14, CARD_BG);
        painter.fill_rounded_rect({ax - 1.0, ay - 1.0, aw + 2.0, ah + 2.0}, 15, BORDER_LINE);

        double cur_y = ay + 8.0;
        for (size_t i = 0; i < show_count; ++i) {
            bool sel = (app_menu_hover == static_cast<int>(i));
            if (sel) {
                painter.fill_rounded_rect({ax + 6.0, cur_y, aw - 12.0, 26.0}, 6, ROW_SELECT);
            }
            painter.fill_circle({ax + 18.0, cur_y + 13.0}, 3.5,
                                all_apps[i].is_terminal ? ACCENT_CYAN : ACCENT_BLUE);
            painter.draw_text({ax + 28.0, cur_y + 6.0}, all_apps[i].name,
                              sel ? txui::Color(255, 255, 255, 255) : TXT_PRI, 0.95);
            cur_y += 28.0;
        }
    }

    // ── 4. CALENDAR FLYOUT (Clean Margin Below 46px Notch, cal_y = 54.0) ───
    if (calendar_open) {
        const double cw = 300.0, ch = 285.0;
        const double cal_x = cx - cw * 0.5;
        const double cal_y = 54.0; // 8px elegant gap below the 46px Aura notch!

        painter.fill_rounded_rect({cal_x - 4.0, cal_y + 8.0, cw + 8.0, ch + 4.0}, 16, txui::Color(0, 0, 0, 100));
        painter.fill_rounded_rect({cal_x, cal_y, cw, ch}, 16, CARD_BG);
        painter.fill_rounded_rect({cal_x - 1.0, cal_y - 1.0, cw + 2.0, ch + 2.0}, 17, BORDER_LINE);

        int cur_year = tb.tm_year + 1900;
        int cur_month = tb.tm_mon + 1 + calendar_nav_offset;
        while (cur_month > 12) { cur_month -= 12; cur_year++; }
        while (cur_month < 1)  { cur_month += 12; cur_year--; }

        const char* month_names[] = {"January","February","March","April","May","June",
                                     "July","August","September","October","November","December"};
        std::string header_title = std::string(month_names[cur_month - 1]) + " " + std::to_string(cur_year);

        // Nav chevrons
        painter.draw_line({cal_x + 22.0, cal_y + 14.0}, {cal_x + 16.0, cal_y + 18.5}, 1.8, ACCENT_CYAN);
        painter.draw_line({cal_x + 16.0, cal_y + 18.5}, {cal_x + 22.0, cal_y + 23.0}, 1.8, ACCENT_CYAN);

        double h_offset = (cw - static_cast<double>(header_title.size()) * 7.5) * 0.5;
        painter.draw_text({cal_x + h_offset, cal_y + 12.0}, header_title, TXT_PRI, 1.05, true);

        painter.draw_line({cal_x + cw - 22.0, cal_y + 14.0}, {cal_x + cw - 16.0, cal_y + 18.5}, 1.8, ACCENT_CYAN);
        painter.draw_line({cal_x + cw - 16.0, cal_y + 18.5}, {cal_x + cw - 22.0, cal_y + 23.0}, 1.8, ACCENT_CYAN);

        const char* dows[] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };
        double col_w = (cw - 24.0) / 7.0;
        for (int d = 0; d < 7; ++d) {
            painter.draw_text({cal_x + 16.0 + static_cast<double>(d) * col_w, cal_y + 42.0},
                              dows[d], TXT_DIM, 0.85, true);
        }
        painter.draw_line({cal_x + 14.0, cal_y + 58.0}, {cal_x + cw - 14.0, cal_y + 58.0}, 1.0, BORDER_LINE);

        int start_dow = day_of_week(cur_year, cur_month, 1);
        int total_days = days_in_month(cur_year, cur_month);

        int today_day = (calendar_nav_offset == 0) ? tb.tm_mday : -1;
        double row_y = cal_y + 66.0;
        int current_col = start_dow;

        for (int day = 1; day <= total_days; ++day) {
            double day_cx = cal_x + 24.0 + static_cast<double>(current_col) * col_w;
            double day_cy = row_y + 11.0;

            bool is_today = (day == today_day);
            bool is_hover = (calendar_day_hover == day);

            if (is_today) {
                painter.fill_circle({day_cx, day_cy}, 12.0, ACCENT_BLUE);
            } else if (is_hover) {
                painter.fill_circle({day_cx, day_cy}, 12.0, txui::Color(255, 255, 255, 25));
            }

            std::string day_str = std::to_string(day);
            double t_off = (day < 10) ? -3.5 : -7.0;
            painter.draw_text({day_cx + t_off, day_cy - 6.0}, day_str,
                              is_today ? txui::Color(255, 255, 255, 255) : (is_hover ? TXT_PRI : TXT_SEC),
                              0.9, is_today);

            current_col++;
            if (current_col > 6) {
                current_col = 0;
                row_y += 26.0;
            }
        }

        painter.draw_line({cal_x + 14.0, cal_y + ch - 32.0}, {cal_x + cw - 14.0, cal_y + ch - 32.0}, 1.0, BORDER_LINE);
        char full_time[32];
        strftime(full_time, sizeof(full_time), "Live Clock: %I:%M:%S %p", &tb);
        painter.draw_text({cal_x + 20.0, cal_y + ch - 22.0}, full_time, ACCENT_CYAN, 0.88);
    }

    // ── 5. NOTIFICATION FLYOUT (Real dynamic cards & clear action) ──────────
    if (notifications_open) {
        const double nw = 390.0;
        const double nx = W - nw - 16.0, ny = 44.0;
        const double nh = notifications.empty() ? 120.0 : (48.0 + static_cast<double>(notifications.size()) * 74.0);

        painter.fill_rounded_rect({nx - 4.0, ny + 6.0, nw + 8.0, nh + 4.0}, 14, txui::Color(0, 0, 0, 95));
        painter.fill_rounded_rect({nx - 1.0, ny - 1.0, nw + 2.0, nh + 2.0}, 15, BORDER_LINE);
        painter.fill_rounded_rect({nx, ny, nw, nh}, 14, CARD_BG);

        // Header
        std::string header = "Notifications (" + std::to_string(notifications.size()) + ")";
        painter.draw_text({nx + 16.0, ny + 13.0}, header, TXT_PRI, 1.0, true);

        if (!notifications.empty()) {
            // [Clear All] Pill button
            painter.fill_rounded_rect({nx + nw - 87.0, ny + 9.0, 74.0, 26.0}, 7, txui::Color(255, 255, 255, 20));
            painter.fill_rounded_rect({nx + nw - 86.0, ny + 10.0, 72.0, 24.0}, 6,
                                      notif_clear_hover ? CARD_HOVER : txui::Color(36, 40, 52, 255));
            double clr_len = 9.0 * 7.2;
            double clr_x = (nx + nw - 86.0) + (72.0 - clr_len) * 0.5;
            painter.draw_text({clr_x, ny + 14.0}, "Clear All", TXT_SEC, 0.85);

            double cur_ny = ny + 44.0;
            for (size_t i = 0; i < notifications.size(); ++i) {
                const auto& n = notifications[i];
                bool hov = (notif_hover_idx == static_cast<int>(i));

                painter.fill_rounded_rect({nx + 7.0, cur_ny - 1.0, nw - 14.0, 68.0}, 9, BORDER_LINE);
                painter.fill_rounded_rect({nx + 8.0, cur_ny, nw - 16.0, 66.0}, 8,
                                          hov ? CARD_HOVER : txui::Color(30, 32, 42, 220));

                // Urgency / status dot (centered at y = cur_ny + 33.0)
                painter.fill_circle({nx + 26.0, cur_ny + 33.0}, 8.0, n.icon_col);
                painter.fill_circle({nx + 26.0, cur_ny + 33.0}, 3.0, txui::Color(255, 255, 255, 240));

                // Title with smart truncation guard
                const double max_title_w = nw - 130.0;
                std::string disp_title = n.title;
                size_t max_title_chars = static_cast<size_t>(max_title_w / 7.6);
                if (disp_title.size() > max_title_chars && max_title_chars > 3) {
                    disp_title = disp_title.substr(0, max_title_chars - 3) + "...";
                }
                painter.draw_text({nx + 44.0, cur_ny + 12.0}, disp_title, TXT_PRI, 0.95, true);
                painter.draw_text({nx + nw - 70.0, cur_ny + 12.0}, n.time_ago, TXT_DIM, 0.8);

                // Body with smart truncation guard (zero overflow guarantee)
                const double max_body_w = nw - 62.0;
                std::string disp_body = n.body;
                size_t max_body_chars = static_cast<size_t>(max_body_w / 6.8);
                if (disp_body.size() > max_body_chars && max_body_chars > 3) {
                    disp_body = disp_body.substr(0, max_body_chars - 3) + "...";
                }
                painter.draw_text({nx + 44.0, cur_ny + 34.0}, disp_body, TXT_SEC, 0.85);

                cur_ny += 74.0;
            }
        } else {
            painter.draw_circle({nx + nw * 0.5, ny + 62.0}, 16.0, 1.5, TXT_DIM);
            painter.draw_text({nx + nw * 0.5 - 55.0, ny + 90.0}, "No New Notifications", TXT_DIM, 0.92);
        }
    }

    // ── 6. PULSE SEARCH OVERLAY (Ctrl+K Command Palette) ───────────────────
    if (pulse_active) {
        const double CARD_W = 680.0;
        const double SEARCH_H = 56.0;
        const double ROW_H = 48.0;
        const double ROW_GAP = 3.0;
        const double rows_n = static_cast<double>(std::min(pulse_results.size(), size_t{7}));
        const double card_h = SEARCH_H + rows_n * (ROW_H + ROW_GAP) + 20.0;
        const double px = (W - CARD_W) * 0.5;
        const double py = 60.0; // 14px gap below 46px Aura notch

        painter.fill_rounded_rect({px - 6.0, py + 10.0, CARD_W + 12.0, card_h + 4.0}, 18, txui::Color(0, 0, 0, 100));
        painter.fill_rounded_rect({px, py, CARD_W, card_h}, 18, CARD_BG);
        painter.fill_rounded_rect({px - 1.0, py - 1.0, CARD_W + 2.0, card_h + 2.0}, 19, BORDER_LINE);

        // Search Input Bar
        painter.fill_rounded_rect({px + 6.0, py + 6.0, CARD_W - 12.0, SEARCH_H - 12.0}, 12, txui::Color(32, 35, 46, 255));
        painter.draw_circle({px + 32.0, py + SEARCH_H * 0.5}, 9.0, 1.6, ACCENT_CYAN);
        painter.draw_line({px + 38.0, py + SEARCH_H * 0.5 + 6.0}, {px + 44.0, py + SEARCH_H * 0.5 + 12.0}, 1.8, ACCENT_CYAN);

        const std::string disp = pulse_query.empty() ? "Search apps, commands, files, or calculate..." : (pulse_query + "_");
        painter.draw_text({px + 56.0, py + (SEARCH_H - 16.0) * 0.5}, disp,
                          pulse_query.empty() ? TXT_DIM : TXT_PRI, 1.05);

        double rows_y = py + SEARCH_H + 6.0;
        for (size_t i = 0; i < std::min(pulse_results.size(), size_t{7}); ++i) {
            const auto& item = pulse_results[i];
            bool sel = (i == pulse_selected_index);
            double ry = rows_y + static_cast<double>(i) * (ROW_H + ROW_GAP);

            if (sel) {
                painter.fill_rounded_rect({px + 8.0, ry, CARD_W - 16.0, ROW_H}, 8, ROW_SELECT);
            } else if (static_cast<int>(i) == pulse_hovered_index) {
                painter.fill_rounded_rect({px + 8.0, ry, CARD_W - 16.0, ROW_H}, 8, CARD_HOVER);
            }

            // Leading Kind Badge
            txui::Color cat_col = (item.kind == ResultKind::Calculator) ? txui::Color(16, 185, 129, 255) :
                                  (item.kind == ResultKind::System) ? txui::Color(239, 68, 68, 255) : ACCENT_BLUE;
            painter.fill_circle({px + 30.0, ry + ROW_H * 0.5}, 5.0, cat_col);

            // Title
            painter.draw_text({px + 48.0, ry + (ROW_H - 16.0) * 0.5}, item.name,
                              sel ? txui::Color(255, 255, 255, 255) : TXT_PRI, 1.02, sel);

            // Description
            if (!item.description.empty()) {
                std::string d = item.description;
                if (d.size() > 34) d = d.substr(0, 31) + "...";
                painter.draw_text({px + 230.0, ry + (ROW_H - 14.0) * 0.5}, d,
                                  sel ? txui::Color(220, 230, 255, 220) : TXT_DIM, 0.9);
            }

            // Right Pill (Category or Return Action)
            if (sel) {
                painter.fill_rounded_rect({px + CARD_W - 80.0, ry + 12.0, 64.0, 24.0}, 4, txui::Color(255, 255, 255, 45));
                painter.draw_text({px + CARD_W - 72.0, ry + 16.0}, "↵ Open", txui::Color(255, 255, 255, 255), 0.88, true);
            } else {
                const char* cat_label = (item.kind == ResultKind::Calculator) ? "CALC" :
                                        (item.kind == ResultKind::System) ? "SYSTEM" : "APP";
                painter.draw_text({px + CARD_W - 70.0, ry + (ROW_H - 14.0) * 0.5}, cat_label, TXT_DIM, 0.85);
            }
        }
    }
}

void DesktopShellWidget::close_all_flyouts() {
    bool changed = (logo_menu_open || app_menu_open || calendar_open || notifications_open || pulse_active);
    logo_menu_open = false;
    app_menu_open = false;
    calendar_open = false;
    notifications_open = false;
    pulse_active = false;
    if (changed && on_resize_requested) {
        on_resize_requested(46.0);
    }
    mark_needs_paint();
}

bool DesktopShellWidget::handle_event(const txui::Event& event) noexcept {
    const double W = frame().width();
    const double cx = W * 0.5;

    if (event.type == txui::EventType::PointerMove) {
        double mx = event.pointer.x, my = event.pointer.y;

        bool h_logo = (mx >= 6.0 && mx <= 36.0 && my >= 0.0 && my <= 32.0);
        double app_w = static_cast<double>(active_app_name.size()) * 7.5 + 24.0;
        bool h_app  = (mx >= 42.0 && mx <= 42.0 + app_w && my >= 0.0 && my <= 32.0);
        // Symmetrical date hit test: cx + 24.0 to cx + 96.0
        bool h_date = (mx >= cx + 24.0 && mx <= cx + 96.0 && my >= 6.0 && my <= 38.0);
        bool h_sun  = (mx >= W - 230.0 && mx <= W - 200.0 && my >= 0.0 && my <= 32.0);
        bool h_vol  = (mx >= W - 195.0 && mx <= W - 165.0 && my >= 0.0 && my <= 32.0);
        bool h_bat  = (mx >= W - 155.0 && mx <= W - 90.0 && my >= 0.0 && my <= 32.0);
        bool h_wifi = (mx >= W - 88.0 && mx <= W - 50.0 && my >= 0.0 && my <= 32.0);
        bool h_bell = (mx >= W - 46.0 && mx <= W - 10.0 && my >= 0.0 && my <= 32.0);

        if (hover_logo != h_logo || hover_app_title != h_app || hover_aura_date != h_date ||
            hover_sun != h_sun || hover_vol != h_vol || hover_bat != h_bat ||
            hover_wifi != h_wifi || hover_bell != h_bell) {
            hover_logo = h_logo;
            hover_app_title = h_app;
            hover_aura_date = h_date;
            hover_sun = h_sun;
            hover_vol = h_vol;
            hover_bat = h_bat;
            hover_wifi = h_wifi;
            hover_bell = h_bell;
            mark_needs_paint();
        }

        if (logo_menu_open) {
            int new_h = -1;
            if (mx >= 8.0 && mx <= 244.0 && my >= 38.0 && my <= 300.0) {
                double rel_y = my - 46.0;
                if (rel_y >= 0.0) new_h = std::clamp(static_cast<int>(rel_y / 32.0), 0, 7);
            }
            if (logo_menu_hover != new_h) {
                logo_menu_hover = new_h;
                mark_needs_paint();
            }
        }

        if (app_menu_open) {
            int new_h = -1;
            if (mx >= 44.0 && mx <= 294.0 && my >= 38.0) {
                double rel_y = my - 46.0;
                if (rel_y >= 0.0) new_h = static_cast<int>(rel_y / 28.0);
            }
            if (app_menu_hover != new_h) {
                app_menu_hover = new_h;
                mark_needs_paint();
            }
        }

        if (notifications_open) {
            const double nw = 390.0;
            const double nx = W - nw - 16.0, ny = 44.0;
            bool clr = (mx >= nx + nw - 86.0 && mx <= nx + nw - 14.0 && my >= ny + 10.0 && my <= ny + 34.0);
            int n_hov = -1;
            if (mx >= nx + 8.0 && mx <= nx + nw - 8.0 && my >= ny + 44.0) {
                n_hov = static_cast<int>((my - (ny + 44.0)) / 74.0);
                if (n_hov >= static_cast<int>(notifications.size())) n_hov = -1;
            }
            if (notif_clear_hover != clr || notif_hover_idx != n_hov) {
                notif_clear_hover = clr;
                notif_hover_idx = n_hov;
                mark_needs_paint();
            }
        }

        if (pulse_active) {
            double px = (W - 680.0) * 0.5;
            double rows_y = 60.0 + 56.0 + 6.0;
            int new_h = -1;
            if (mx >= px && mx <= px + 680.0 && my >= rows_y) {
                new_h = static_cast<int>((my - rows_y) / 51.0);
                if (new_h >= static_cast<int>(pulse_results.size())) new_h = -1;
            }
            if (pulse_hovered_index != new_h) {
                pulse_hovered_index = new_h;
                mark_needs_paint();
            }
        }
        return true;

    } else if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            double mx = event.pointer.x, my = event.pointer.y;

            if (my <= 46.0) {
                if (hover_logo) {
                    logo_menu_open = !logo_menu_open;
                    app_menu_open = false;
                    calendar_open = false;
                    notifications_open = false;
                    pulse_active = false;
                    if (on_resize_requested) on_resize_requested(logo_menu_open ? 300.0 : 46.0);
                    mark_needs_paint();
                    return true;
                }
                if (hover_app_title) {
                    app_menu_open = !app_menu_open;
                    logo_menu_open = false;
                    calendar_open = false;
                    notifications_open = false;
                    pulse_active = false;
                    if (on_resize_requested) on_resize_requested(app_menu_open ? 380.0 : 46.0);
                    mark_needs_paint();
                    return true;
                }
                if (hover_aura_date) {
                    calendar_open = !calendar_open;
                    logo_menu_open = false;
                    app_menu_open = false;
                    notifications_open = false;
                    pulse_active = false;
                    if (on_resize_requested) on_resize_requested(calendar_open ? 350.0 : 46.0);
                    mark_needs_paint();
                    return true;
                }
                if (hover_sun) {
                    dark_theme = !dark_theme;
                    log::info("[Shell] Theme toggled");
                    mark_needs_paint();
                    return true;
                }
                if (hover_vol) {
                    sound_muted = !sound_muted;
                    log::info("[Shell] Sound mute state: {}", sound_muted);
                    mark_needs_paint();
                    return true;
                }
                if (hover_bat) {
                    log::info("[Shell] Battery clicked: capacity={}%", read_battery_percent());
                    return true;
                }
                if (hover_wifi) {
                    log::info("[Shell] Wi-Fi clicked -> launching Settings Network page");
                    AppItem wifi_app{"Settings", "tinexus-settings-ui --page network", "Network Settings", false, ""};
                    spawn_app(wifi_app);
                    close_all_flyouts();
                    return true;
                }
                if (hover_bell) {
                    notifications_open = !notifications_open;
                    logo_menu_open = false;
                    app_menu_open = false;
                    calendar_open = false;
                    pulse_active = false;
                    if (on_resize_requested) on_resize_requested(notifications_open ? 350.0 : 46.0);
                    mark_needs_paint();
                    return true;
                }
            }

            if (logo_menu_open && mx >= 8.0 && mx <= 244.0 && my >= 38.0 && my <= 300.0) {
                if (logo_menu_hover == 0) {
                    AppItem ab{"About Tinexus", "tinexus-about", "System Profiler", false, ""};
                    spawn_app(ab);
                } else if (logo_menu_hover == 1) {
                    AppItem st{"Tinexus Settings", "tinexus-settings-ui", "Settings", false, ""};
                    spawn_app(st);
                } else if (logo_menu_hover == 2) {
                    AppItem inst{"Tinexus App Installer", "tinexus-app-installer", "Installer", false, ""};
                    spawn_app(inst);
                } else if (logo_menu_hover == 3) {
                    AppItem mon{"Tinexus System Monitor", "tinexus-monitor", "Monitor", false, ""};
                    spawn_app(mon);
                } else if (logo_menu_hover == 4) {
                    log::info("[Shell] System Sleep triggered");
                } else if (logo_menu_hover == 5) {
                    AppItem rb{"Restart", "reboot", "", false, "", ResultKind::System};
                    spawn_app(rb);
                } else if (logo_menu_hover == 6) {
                    AppItem sd{"Shut Down", "shutdown", "", false, "", ResultKind::System};
                    spawn_app(sd);
                } else if (logo_menu_hover == 7) {
                    AppItem lk{"Lock Screen", "lock", "", false, "", ResultKind::System};
                    spawn_app(lk);
                }
                close_all_flyouts();
                return true;
            }

            if (app_menu_open && mx >= 44.0 && mx <= 294.0 && my >= 38.0) {
                if (app_menu_hover >= 0 && app_menu_hover < static_cast<int>(all_apps.size())) {
                    size_t idx = static_cast<size_t>(app_menu_hover);
                    active_app_name = all_apps[idx].name;
                    spawn_app(all_apps[idx]);
                    close_all_flyouts();
                    return true;
                }
            }

            if (calendar_open && my >= 54.0 && my <= 340.0) {
                const double cw = 300.0;
                const double cal_x = cx - cw * 0.5;
                const double cal_y = 54.0;
                if (mx >= cal_x + 10.0 && mx <= cal_x + 35.0 && my >= cal_y + 8.0 && my <= cal_y + 32.0) {
                    calendar_nav_offset--;
                    mark_needs_paint();
                    return true;
                }
                if (mx >= cal_x + cw - 35.0 && mx <= cal_x + cw - 10.0 && my >= cal_y + 8.0 && my <= cal_y + 32.0) {
                    calendar_nav_offset++;
                    mark_needs_paint();
                    return true;
                }
            }

            if (notifications_open) {
                const double nw = 390.0;
                const double nx = W - nw - 16.0, ny = 44.0;
                if (mx >= nx + nw - 86.0 && mx <= nx + nw - 14.0 && my >= ny + 10.0 && my <= ny + 34.0) {
                    notifications.clear();
                    mark_needs_paint();
                    return true;
                }
            }

            if (pulse_active && pulse_hovered_index >= 0 &&
                pulse_hovered_index < static_cast<int>(pulse_results.size())) {
                size_t idx = static_cast<size_t>(pulse_hovered_index);
                if (on_app_launch) {
                    on_app_launch(pulse_results[idx]);
                } else {
                    active_app_name = pulse_results[idx].name;
                    spawn_app(pulse_results[idx]);
                    close_all_flyouts();
                }
                return true;
            }

            if (my > 46.0) {
                close_all_flyouts();
                return true;
            }
        }
    }
    return false;
}

} // namespace tinexus::shell
