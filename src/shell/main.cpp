#include <txui/window/Window.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <txui/widgets/TextWidget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/theme/Theme.hpp>
#include <common/logger.hpp>
#include <indexer/desktop_entry.hpp>
#include <unistd.h>   // fork, execvp, execlp, setsid
#include <csignal>    // signal, SIGCHLD, SIG_IGN
#include <cstdlib>    // _exit
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>      // for inline calculator
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <atomic>

using namespace tinexus;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Unified search result — covers apps, system actions, and calculator
// ---------------------------------------------------------------------------
enum class ResultKind { App, System, Calculator };

struct AppItem {
    std::string name;
    std::string exec;
    std::string description;
    bool is_terminal{false};
    std::string icon;
    ResultKind  kind{ResultKind::App};
};

// Recent launches — capped at 5, persisted in-memory per session
static std::vector<AppItem> g_recent_launches;
constexpr size_t MAX_RECENT = 5;

// ---------------------------------------------------------------------------
// spawn_app — executes target binary or desktop entry command
// Records launches in g_recent_launches for empty-state display
// ---------------------------------------------------------------------------
static pid_t spawn_app(const AppItem& item) {
    const std::string& raw_exec = item.exec;
    // System actions are handled separately — don't exec them
    if (item.kind == ResultKind::System) {
        const std::string& cmd = raw_exec;
        if (cmd == "lock") {
            log::info("[Launcher] System action: Lock Screen");
            pid_t pid = fork();
            if (pid == 0) {
                setsid();
                execlp("tinexus-lock", "tinexus-lock", nullptr);
                _exit(127);
            }
            return pid;
        } else if (cmd == "shutdown") {
            log::info("[Launcher] System action: Shutdown");
            ::system("systemctl poweroff"); // NOLINT — intentional
            return -1;
        } else if (cmd == "reboot") {
            log::info("[Launcher] System action: Restart");
            ::system("systemctl reboot"); // NOLINT
            return -1;
        } else if (cmd == "sleep") {
            log::info("[Launcher] System action: Sleep");
            ::system("systemctl suspend"); // NOLINT
            return -1;
        } else if (cmd == "logout") {
            log::info("[Launcher] System action: Log Out");
            ::system("loginctl terminate-session ''"); // NOLINT
            return -1;
        }
        return -1;
    }

    std::string clean_exec = indexer::DesktopParser::sanitize_exec(raw_exec);
    if (clean_exec.empty()) return -1;

    // Track recent launch
    auto it = std::find_if(g_recent_launches.begin(), g_recent_launches.end(),
        [&](const AppItem& a) { return a.exec == item.exec; });
    if (it != g_recent_launches.end()) g_recent_launches.erase(it);
    g_recent_launches.insert(g_recent_launches.begin(), item);
    if (g_recent_launches.size() > MAX_RECENT) g_recent_launches.pop_back();

    pid_t pid = fork();
    if (pid < 0) {
        log::error("[Launcher] fork() failed for app '{}'", clean_exec);
        return -1;
    }
    if (pid == 0) {
        setsid();
        if (item.is_terminal) {
            execlp("foot", "foot", "-e", clean_exec.c_str(), nullptr);
            execlp("weston-terminal", "weston-terminal", nullptr);
            _exit(127);
        } else {
            std::vector<std::string> tokens;
            std::istringstream iss(clean_exec);
            std::string token;
            while (iss >> token) tokens.push_back(token);
            if (tokens.empty()) _exit(1);
            std::vector<char*> args;
            for (auto& t : tokens) args.push_back(const_cast<char*>(t.c_str()));
            args.push_back(nullptr);
            execvp(args[0], args.data());
            _exit(127);
        }
    }
    log::info("[Launcher] Spawned '{}' PID={}", clean_exec, pid);
    return pid;
}

// ---------------------------------------------------------------------------
// Inline calculator — evaluate basic arithmetic (no IPC needed, instant)
// ---------------------------------------------------------------------------
static bool try_eval_calc(const std::string& query, double& result) {
    // Only attempt if string looks like arithmetic
    bool has_op  = false;
    bool has_dig = false;
    for (char c : query) {
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') has_dig = true;
        if (c == '+' || c == '-' || c == '*' || c == '/') has_op = true;
        if (!std::isdigit(static_cast<unsigned char>(c)) &&
            c != '+' && c != '-' && c != '*' && c != '/' &&
            c != '.' && c != ' ' && c != '(' && c != ')') {
            return false;
        }
    }
    if (!has_dig || !has_op) return false;

    // Simple two-operand evaluation (extend for full AST in v0.2)
    // Handles: A op B where op is +, -, *, /
    double a{0}, b{0};
    char op{'?'};
    std::istringstream ss(query);
    if (!(ss >> a)) return false;
    if (!(ss >> op)) return false;
    if (!(ss >> b)) return false;
    if (b == 0.0 && op == '/') return false;

    switch (op) {
        case '+': result = a + b; break;
        case '-': result = a - b; break;
        case '*': result = a * b; break;
        case '/': result = a / b; break;
        default:  return false;
    }
    return true;
}

// Convert txui::Key to a printable character
static char key_to_char(txui::Key key, bool shift_pressed) {
    if (key >= txui::Key::A && key <= txui::Key::Z) {
        char c = static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(txui::Key::A)));
        if (shift_pressed) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return c;
    }
    if (key >= txui::Key::N0 && key <= txui::Key::N9) {
        return static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(txui::Key::N0)));
    }
    if (key == txui::Key::Space) return ' ';
    return '\0';
}

// ---------------------------------------------------------------------------
// get_system_actions — always-available OS power/session actions
// ---------------------------------------------------------------------------
static std::vector<AppItem> get_system_actions(const std::string& lq) {
    struct SA { const char* cmd; const char* label; const char* desc; const char* icon; };
    constexpr SA kActions[] = {
        {"lock",     "Lock Screen",  "Lock the current session",  "system-lock-screen"},
        {"shutdown", "Shut Down",    "Power off the computer",    "system-shutdown"},
        {"reboot",   "Restart",      "Restart the computer",      "system-reboot"},
        {"sleep",    "Sleep",        "Suspend to RAM",            "system-suspend"},
        {"logout",   "Log Out",      "End the current session",   "system-log-out"},
    };
    std::vector<AppItem> out;
    for (const auto& a : kActions) {
        std::string cmd(a.cmd), label(a.label);
        if (lq.empty() ||
            cmd.find(lq)   != std::string::npos ||
            label.find(lq) != std::string::npos) {
            out.push_back({a.label, a.cmd, a.desc, false, a.icon, ResultKind::System});
        }
    }
    return out;
}

// Helper to convert string to lowercase
static std::string to_lower(std::string_view sv) {
    std::string res;
    res.reserve(sv.size());
    for (char c : sv) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

// ---------------------------------------------------------------------------
// Load applications from system .desktop directories + fallback apps
// ---------------------------------------------------------------------------
[[maybe_unused]] static std::string icon_to_emoji(const std::string& icon_name, const std::string& app_name) {
    std::string name = to_lower(icon_name);
    std::string app = to_lower(app_name);

    if (name.find("terminal") != std::string::npos || app.find("terminal") != std::string::npos || app.find("foot") != std::string::npos || app.find("alacritty") != std::string::npos)
        return "💻";
    if (name.find("monitor") != std::string::npos || name.find("htop") != std::string::npos || app.find("monitor") != std::string::npos)
        return "📊";
    if (name.find("setting") != std::string::npos || name.find("prefer") != std::string::npos || app.find("setting") != std::string::npos)
        return "⚙️";
    if (name.find("package") != std::string::npos || name.find("software") != std::string::npos || app.find("package") != std::string::npos)
        return "📦";
    if (name.find("file") != std::string::npos || name.find("folder") != std::string::npos || app.find("file") != std::string::npos)
        return "📂";
    if (name.find("browser") != std::string::npos || name.find("firefox") != std::string::npos || name.find("chrome") != std::string::npos || name.find("web") != std::string::npos || name.find("internet") != std::string::npos)
        return "🌐";
    if (name.find("text") != std::string::npos || name.find("edit") != std::string::npos || name.find("note") != std::string::npos || name.find("document") != std::string::npos)
        return "📄";
    if (name.find("calc") != std::string::npos || app.find("calc") != std::string::npos)
        return "🧮";
    if (name.find("mail") != std::string::npos || name.find("thunderbird") != std::string::npos)
        return "✉️";
    if (name.find("music") != std::string::npos || name.find("player") != std::string::npos || name.find("audio") != std::string::npos || name.find("volume") != std::string::npos)
        return "🎵";
    if (name.find("video") != std::string::npos || name.find("vlc") != std::string::npos || name.find("movie") != std::string::npos)
        return "🎬";
    if (name.find("image") != std::string::npos || name.find("photo") != std::string::npos || name.find("gimp") != std::string::npos || name.find("paint") != std::string::npos)
        return "🎨";
    if (name.find("game") != std::string::npos || name.find("steam") != std::string::npos)
        return "🎮";
    if (name.find("development") != std::string::npos || name.find("code") != std::string::npos || name.find("visual-studio") != std::string::npos || name.find("developer") != std::string::npos)
        return "🛠️";
    
    return "🔹";
}

// ---------------------------------------------------------------------------
// Load applications from system .desktop directories + fallback apps
// ---------------------------------------------------------------------------
static std::vector<AppItem> load_system_apps() {
    std::vector<AppItem> apps;

    // Built-in system defaults
    apps.push_back({"Foot Terminal", "foot", "Fast Wayland Terminal Emulator", true, "utilities-terminal"});
    apps.push_back({"Weston Terminal", "weston-terminal", "Wayland Demo Terminal", true, "utilities-terminal"});
    apps.push_back({"Alacritty", "alacritty", "GPU Accelerated Terminal", true, "utilities-terminal"});
    apps.push_back({"Tinexus System Monitor", "tinexus-monitor", "Platform Resource & Process Monitor", false, "utilities-system-monitor"});
    apps.push_back({"Tinexus Settings", "tinexus-settings-ui", "System Configuration & Control Center", false, "preferences-desktop"});
    apps.push_back({"Tinexus Package Manager", "tinexus-pkg", "Package Installer & Software Manager", false, "system-software-install"});
    apps.push_back({"Tinexus Files", "tinexus-files", "Lightweight Desktop File Manager", false, "system-file-manager"});

    // Scan system application directories
    std::vector<fs::path> search_dirs = {
        "/usr/share/applications",
        "/usr/local/share/applications"
    };
    const char* home = std::getenv("HOME");
    if (home) {
        search_dirs.push_back(fs::path(home) / ".local" / "share" / "applications");
    }

    for (const auto& dir : search_dirs) {
        if (!fs::exists(dir)) continue;
        try {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
                    auto parsed = indexer::DesktopParser::parse_file(entry.path());
                    if (parsed && !parsed->no_display && !parsed->exec.empty()) {
                        // Avoid duplicates by exec or name
                        bool exists = std::any_of(apps.begin(), apps.end(), [&](const AppItem& item) {
                            return item.name == parsed->name || item.exec == parsed->exec;
                        });
                        if (!exists) {
                            apps.push_back({
                                parsed->name,
                                parsed->exec,
                                parsed->comment.empty() ? parsed->generic_name : parsed->comment,
                                parsed->terminal,
                                parsed->icon
                            });
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            log::warn("[Launcher] Exception scanning app dir {}: {}", dir.string(), e.what());
        }
    }
    
    return apps;
}

// ---------------------------------------------------------------------------
// Build filtered result list from query (apps + system actions + calculator)
// ---------------------------------------------------------------------------
static std::vector<AppItem> build_results(
        const std::string& query,
        const std::vector<AppItem>& all_apps) {
    std::string lq = to_lower(query);
    std::vector<AppItem> results;

    // 1. Inline calculator (highest priority if math detected)
    double calc_result{0};
    if (!lq.empty() && try_eval_calc(lq, calc_result)) {
        std::ostringstream oss;
        if (calc_result == static_cast<long long>(calc_result)) {
            oss << static_cast<long long>(calc_result);
        } else {
            oss << calc_result;
        }
        std::string answer = oss.str();
        results.push_back({"= " + answer, answer,
                           query + " = " + answer,
                           false, "accessories-calculator", ResultKind::Calculator});
    }

    // 2. System actions
    auto sys = get_system_actions(lq);
    results.insert(results.end(), sys.begin(), sys.end());

    // 3. Apps (fuzzy match or recent when empty)
    if (lq.empty()) {
        // Empty query: show recent launches
        for (const auto& r : g_recent_launches) {
            results.push_back(r);
        }
    } else {
        for (const auto& app : all_apps) {
            if (to_lower(app.name).find(lq) != std::string::npos ||
                to_lower(app.exec).find(lq) != std::string::npos ||
                to_lower(app.description).find(lq) != std::string::npos) {
                results.push_back(app);
            }
        }
    }

    return results;
}

// ─────────────────────────────────────────────────────────────────────────────
// LauncherWidget — full custom-painted spotlight palette
// ─────────────────────────────────────────────────────────────────────────────
#include <txui/widgets/Widget.hpp>

namespace launcher_ui {
    constexpr txui::Color BG_PANEL_T {18, 18,  30, 252};
    constexpr txui::Color BG_PANEL_B {12, 12,  22, 252};
    constexpr txui::Color ACCENT     {107,140, 239, 255};
    constexpr txui::Color SEL_APP    { 59,130, 246, 200};
    constexpr txui::Color SEL_SYS    {239, 68,  68, 190};
    constexpr txui::Color SEL_CALC   { 16,185, 129, 200};
    constexpr txui::Color TXT_PRI    {240,240, 248, 255};
    constexpr txui::Color TXT_SEC    {180,180, 210, 200};
    constexpr txui::Color TXT_DIM    {120,120, 150, 150};
    constexpr double PANEL_W   = 680.0;
    constexpr double PANEL_RAD = 20.0;
    constexpr double ROW_H     = 46.0;
    constexpr double ROW_GAP   =  2.0;
}

class LauncherWidget : public txui::Widget {
public:
    std::string          query;
    std::vector<AppItem> results;
    size_t               selected_index{0};

    txui::Size measure_override(const txui::Constraints& c) noexcept override {
        return txui::Size(c.max_width, c.max_height);
    }

    void paint_override(txui::Painter& painter) const noexcept override {
        using namespace launcher_ui;
        const double W  = frame().width();
        const double H  = frame().height();
        
        const double panel_x = frame().x();
        const double panel_y = frame().y();
        const double panel_w = W;
        const double panel_h = H;

        // 1. Panel card with glow + border + glass gradient
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x, panel_y, panel_w, panel_h),
            PANEL_RAD, BG_PANEL_T, BG_PANEL_B);
        painter.fill_rounded_rect(
            txui::Rect(panel_x + 3, panel_y + 1, panel_w - 6, 1),
            0.5, txui::Color(255, 255, 255, 20));
        painter.fill_rounded_rect(
            txui::Rect(panel_x - 1, panel_y - 1, panel_w + 2, panel_h + 2),
            PANEL_RAD + 1, txui::Color(130, 130, 220, 30));
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x, panel_y, panel_w, panel_h),
            PANEL_RAD, BG_PANEL_T, BG_PANEL_B);

        // 2. Header bar
        const double HEADER_H = 32.0;
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x, panel_y, panel_w, HEADER_H),
            PANEL_RAD,
            txui::Color(22, 22, 40, 255),
            txui::Color(16, 16, 30, 255));
        painter.fill_circle(txui::Point(panel_x + 22, panel_y + 16), 5.0, ACCENT);
        painter.draw_glow(txui::Point(panel_x + 22, panel_y + 16), 5.0, 16.0, ACCENT);
        painter.draw_text(txui::Point(panel_x + 36, panel_y + 10),
            "TINEXUS  Command Palette", TXT_DIM, 1.0);
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x + panel_w - 46, panel_y + 8, 36, 17), 4.0,
            txui::Color(50, 50, 70, 200), txui::Color(38, 38, 56, 200));
        painter.draw_text(txui::Point(panel_x + panel_w - 38, panel_y + 11), "Esc", TXT_DIM, 1.0);

        // 3. Search row
        const double SEARCH_H = 58.0;
        const double srch_y = panel_y + HEADER_H;
        painter.fill_gradient_rect(
            txui::Rect(panel_x, srch_y, panel_w, SEARCH_H),
            txui::Color(28, 28, 48, 255),
            txui::Color(22, 22, 38, 255));
        painter.draw_circle(txui::Point(panel_x + 34, srch_y + SEARCH_H * 0.5),
            10.0, 1.5, ACCENT);
        painter.fill_gradient_rounded_rect(
            txui::Rect(panel_x + 40, srch_y + SEARCH_H * 0.5 + 5.0, 9.0, 2.0), 1.0,
            ACCENT, txui::Color(80, 110, 200, 160));
        const std::string display = query.empty()
            ? "Search apps, run commands..." : (query + "_");
        painter.draw_text(
            txui::Point(panel_x + 58, srch_y + (SEARCH_H - 16.0) * 0.5),
            display, query.empty() ? TXT_DIM : TXT_PRI, 1.0);
        painter.fill_gradient_rect(
            txui::Rect(panel_x + 14, srch_y + SEARCH_H - 1, panel_w - 28, 1),
            txui::Color(107, 140, 239, 50),
            txui::Color(107, 140, 239, 0), true);

        // 4. Result rows
        if (!results.empty()) {
            const size_t n_rows = std::min(results.size(), size_t{7});
            const double rows_y = srch_y + SEARCH_H + 8.0;
            for (size_t i = 0; i < n_rows; ++i) {
                const auto&  item = results[i];
                const bool   sel  = (i == selected_index);
                const double ry   = rows_y + static_cast<double>(i) * (ROW_H + ROW_GAP);
                txui::Color  cat  =
                    item.kind == ResultKind::Calculator ? SEL_CALC :
                    item.kind == ResultKind::System     ? SEL_SYS  : SEL_APP;

                if (sel) {
                    painter.fill_gradient_rounded_rect(
                        txui::Rect(panel_x + 8, ry + 1, panel_w - 16, ROW_H - 2), 10.0,
                        txui::Color(cat.r(), cat.g(), cat.b(), 52),
                        txui::Color(cat.r(), cat.g(), cat.b(), 22));
                    painter.fill_gradient_rounded_rect(
                        txui::Rect(panel_x + 10, ry + 8, 3, ROW_H - 16), 1.5,
                        cat, txui::Color(cat.r(), cat.g(), cat.b(), 110));
                }

                const double icx = panel_x + 34.0;
                const double icy = ry + ROW_H * 0.5;
                if (sel) {
                    painter.fill_circle(txui::Point(icx, icy), 13.0,
                        txui::Color(cat.r(), cat.g(), cat.b(), 45));
                    painter.draw_circle(txui::Point(icx, icy), 13.0, 1.0,
                        txui::Color(cat.r(), cat.g(), cat.b(), 150));
                } else {
                    painter.draw_circle(txui::Point(icx, icy), 11.0, 1.0,
                        txui::Color(80, 80, 130, 70));
                }
                painter.fill_circle(txui::Point(icx, icy), 3.5,
                    sel ? cat : txui::Color(110, 110, 160, 130));

                painter.draw_text(
                    txui::Point(panel_x + 56, ry + (ROW_H - 16.0) * 0.5),
                    item.name, sel ? TXT_PRI : TXT_SEC, 1.0);

                if (!item.description.empty()) {
                    const size_t max_d = 30;
                    std::string  desc  = item.description.size() > max_d
                        ? item.description.substr(0, max_d) + "..." : item.description;
                    const double dx = panel_x + panel_w
                        - static_cast<double>(desc.size()) * 8.0 - 34.0;
                    if (dx > panel_x + panel_w * 0.55) {
                        painter.draw_text(txui::Point(dx, ry + (ROW_H - 14.0) * 0.5),
                            desc, TXT_DIM, 0.9);
                    }
                }
                if (i < 9) {
                    painter.draw_text(
                        txui::Point(panel_x + panel_w - 22, ry + (ROW_H - 16.0) * 0.5),
                        std::to_string(i + 1), TXT_DIM, 1.0);
                }
            }
        }

        // 8. Bottom tip row
        const double tip_y = panel_y + panel_h + 14.0;
        const char* tips[] = {"Up/Down: navigate", "Enter: launch", "Tab: cycle", nullptr};
        double tip_x = (panel_x + panel_w * 0.5) - 200.0;
        for (int ti = 0; tips[ti]; ++ti) {
            std::string s(tips[ti]);
            painter.draw_text(txui::Point(tip_x, tip_y), s,
                txui::Color(120, 120, 155, 100), 1.0);
            tip_x += static_cast<double>(s.size()) * 8.0 + 20.0;
        }
    }
};

static std::atomic<bool> g_toggle_search{false};

// ---------------------------------------------------------------------------
// ipc_listener_thread — subscribes to LAUNCHER_OPEN (topic 1000) via ipcd
// and sets g_toggle_search when LAUNCHER_SHOW (1004) arrives.
// ---------------------------------------------------------------------------
void ipc_listener_thread() {
    // Retry loop — ipcd may not be ready yet at shell startup
    int server_fd = -1;
    for (int attempt = 0; attempt < 30 && server_fd < 0; ++attempt) {
        server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (server_fd < 0) { ::sleep(1); continue; }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        char path[108]; // sun_path max is 108 bytes
        snprintf(path, sizeof(path), "/run/user/%d/tinexus/ipc.sock",
                 static_cast<int>(getuid()));
        memcpy(addr.sun_path, path, strlen(path) + 1);

        if (connect(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            close(server_fd);
            server_fd = -1;
            ::sleep(1);
        }
    }
    if (server_fd < 0) {
        log::warn("[Shell] Could not connect to ipcd — Ctrl+K will not work");
        return;
    }

    // Send SYS_SUBSCRIBE_TOPIC (msg_type=2) with payload = uint16_t 1000 (LAUNCHER_OPEN)
#pragma pack(push, 1)
    struct IpcHeader {
        uint32_t magic      = 0x544E5853;
        uint16_t version    = 0x0100;
        uint16_t msg_type;
        uint16_t flags      = 0;
        uint32_t sequence_id = 0;
        uint32_t payload_len;
        uint32_t checksum   = 0;
    };
#pragma pack(pop)
    static_assert(sizeof(IpcHeader) == 22, "header size mismatch");

    uint16_t topic = 1000; // LAUNCHER_OPEN
    IpcHeader sub_hdr;
    sub_hdr.msg_type    = 2; // SYS_SUBSCRIBE_TOPIC
    sub_hdr.payload_len = sizeof(topic);
    send(server_fd, &sub_hdr, sizeof(sub_hdr), MSG_NOSIGNAL);
    send(server_fd, &topic,   sizeof(topic),   MSG_NOSIGNAL);
    log::info("[Shell] Subscribed to LAUNCHER_OPEN (1000) via ipcd");

    // Receive loop — read full IPC frames and react to LAUNCHER_SHOW (1004)
    while (true) {
        IpcHeader rx_hdr;
        ssize_t got = 0;
        auto* raw = reinterpret_cast<uint8_t*>(&rx_hdr);
        while (got < static_cast<ssize_t>(sizeof(rx_hdr))) {
            ssize_t n = recv(server_fd, raw + got, sizeof(rx_hdr) - static_cast<size_t>(got), 0);
            if (n <= 0) goto done;
            got += n;
        }
        // Drain payload bytes
        if (rx_hdr.payload_len > 0) {
            std::vector<uint8_t> payload(rx_hdr.payload_len);
            ssize_t pg = 0;
            while (pg < static_cast<ssize_t>(rx_hdr.payload_len)) {
                ssize_t n = recv(server_fd, payload.data() + pg,
                                 rx_hdr.payload_len - static_cast<size_t>(pg), 0);
                if (n <= 0) goto done;
                pg += n;
            }
        }
        if (rx_hdr.msg_type == 1004 /* LAUNCHER_SHOW */) {
            g_toggle_search.store(true);
            log::info("[Shell] LAUNCHER_SHOW received — toggling island");
        }
    }
done:
    close(server_fd);
    log::warn("[Shell] ipcd connection lost");
}

class IslandWidget : public LauncherWidget {
public:
    bool is_search_mode = false;

    void paint_override(txui::Painter& painter) const noexcept override {
        const auto& f = this->frame();
        const double cx  = f.x() + f.width()  / 2.0;
        const double cy  = f.y() + f.height() / 2.0;

        if (is_search_mode) {
            // Full search view — delegate to LauncherWidget
            painter.fill_rounded_rect(f, 20, txui::theme::SURFACE_ELEVATED);
            LauncherWidget::paint_override(painter);
            return;
        }

        // ── Pill background ──────────────────────────────────────────────
        painter.fill_rounded_rect(f, static_cast<int>(f.height() / 2.0),
                                  txui::theme::SURFACE_ELEVATED);

        // ── LEFT: time + wifi ────────────────────────────────────────────
        time_t now = time(nullptr);
        struct tm tm_buf;
#if defined(_WIN32)
        localtime_s(&tm_buf, &now);
#else
        localtime_r(&now, &tm_buf);
#endif
        char time_str[16];
        strftime(time_str, sizeof(time_str), "%H:%M", &tm_buf);
        painter.draw_text({f.x() + 16.0, cy + 6.0},
                          time_str, txui::theme::TXT_PRIMARY, 15);
        // Wifi bars (3 small rectangles)
        double wx = f.x() + 74.0;
        const double wy_base = cy + 6.0;
        const txui::Color wifi_col{100, 210, 100, 220};
        // 3 bars of increasing height
        for (int b = 0; b < 3; ++b) {
            double bh = 4.0 + static_cast<double>(b) * 3.0;
            painter.fill_rounded_rect(
                {wx + static_cast<double>(b) * 6.0,
                 wy_base - bh + 2.0,
                 4.0, bh},
                1, wifi_col);
        }

        // ── CENTER: profile avatar ───────────────────────────────────────
        const double r = f.height() / 2.0 - 5.0;
        painter.fill_rounded_rect(
            {cx - r, f.y() + 5.0, r * 2.0, r * 2.0},
            static_cast<int>(r), txui::theme::ACCENT_COLOR);
        // Initials
        painter.draw_text({cx - 5.0, cy + 6.0},
                          "T", txui::Color(255, 255, 255, 230), 14);

        // ── RIGHT: battery icon + percentage ─────────────────────────────
        const double batt_pct = 85.0; // TODO: read from sysfs in Phase D
        const txui::Color batt_col = batt_pct > 20.0
            ? txui::Color{100, 220, 130, 220}
            : txui::Color{240, 80, 80, 220};
        // Battery outline
        const double bx = f.x() + f.width() - 78.0;
        const double by = cy - 7.0;
        painter.fill_rounded_rect({bx, by, 22.0, 14.0}, 2,
                                  txui::Color{80, 80, 100, 180});
        // Battery nub
        painter.fill_rounded_rect({bx + 22.0, by + 3.5, 3.0, 7.0}, 1,
                                  txui::Color{80, 80, 100, 180});
        // Battery fill
        painter.fill_rounded_rect(
            {bx + 2.0, by + 2.0,
             (18.0 * batt_pct / 100.0), 10.0},
            1, batt_col);
        // Percentage text
        char pct_str[8];
        snprintf(pct_str, sizeof(pct_str), "%.0f%%",
                 static_cast<double>(batt_pct));
        painter.draw_text({bx + 26.0, cy + 6.0},
                          pct_str, txui::theme::TXT_PRIMARY, 13);
    }
};

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    log::set_component_name("shell");
    log::info("[Shell] Tinexus Unified Shell starting...");
    signal(SIGCHLD, SIG_IGN);

    auto window = txui::Window::create(360, 48, "Tinexus Launcher", true);
    std::thread(ipc_listener_thread).detach();
    if (!window || !window->is_wayland_connected()) {
        log::error("[Shell] Failed to connect to Wayland display! Exiting.");
        return 1;
    }

    std::vector<AppItem> all_apps = load_system_apps();
    std::string          query;
    size_t               selected_index = 0;
    std::vector<AppItem> current_results;

    auto launcher_widget = txui::make_ref<IslandWidget>();
    window->set_root_widget(txui::Ref<txui::Widget>(launcher_widget.get()));

    auto sync = [&]() {
        current_results = build_results(query, all_apps);
        if (current_results.empty())
            selected_index = 0;
        else if (selected_index >= current_results.size())
            selected_index = current_results.size() - 1;
        launcher_widget->query          = query;
        launcher_widget->selected_index = selected_index;
        launcher_widget->results        = current_results;
        launcher_widget->mark_needs_paint();
    };
    sync();

    // Initial frame presentation
    window->present();

    bool running = true;
    bool needs_redraw = false;

    // Use a simple exponential smoothing or spring for size
    double current_w = 360.0;
    double current_h = 48.0;
    double target_w = 360.0;
    double target_h = 48.0;
    bool animating = false;

    while (running && !window->should_close()) {
        if (g_toggle_search.exchange(false)) {
            launcher_widget->is_search_mode = !launcher_widget->is_search_mode;
            if (launcher_widget->is_search_mode) {
                target_w = 800.0;
                target_h = 600.0;
            } else {
                target_w = 360.0;
                target_h = 48.0;
            }
            animating = true;
            needs_redraw = true;
        }

        if (animating) {
            // Spring/lerp to target
            double dw = target_w - current_w;
            double dh = target_h - current_h;
            current_w += dw * 0.4;
            current_h += dh * 0.4;
            
            if (std::abs(dw) < 1.0 && std::abs(dh) < 1.0) {
                current_w = target_w;
                current_h = target_h;
                animating = false;
            }
            
            window->resize(static_cast<uint32_t>(current_w), static_cast<uint32_t>(current_h));
            launcher_widget->mark_needs_paint();
            needs_redraw = true;
        }

        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else if (event.type == txui::EventType::KeyDown) {
                needs_redraw = true;
                const bool shift = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift);
                const bool ctrl  = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Ctrl);
                const bool alt   = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Alt);

                if (event.keyboard.key == txui::Key::Escape) {
                    if (launcher_widget->is_search_mode) {
                        launcher_widget->is_search_mode = false;
                        target_w = 360.0;
                        target_h = 48.0;
                        animating = true;
                        launcher_widget->mark_needs_paint();
                    } else {
                        running = false;
                    }
                } else if (event.keyboard.key == txui::Key::Enter) {
                    if (alt) {
                        if (!current_results.empty() && selected_index < current_results.size()) {
                            std::string cmd = "echo -n '" + current_results[selected_index].exec + "' | wl-copy";
                            ::system(cmd.c_str()); // NOLINT
                            running = false;
                        }
                    } else if (!current_results.empty() && selected_index < current_results.size()) {
                        spawn_app(current_results[selected_index]); running = false;
                    } else if (!query.empty()) {
                        AppItem ci; ci.name = query; ci.exec = query; ci.kind = ResultKind::App;
                        spawn_app(ci); running = false;
                    }
                } else if (event.keyboard.key == txui::Key::Up) {
                    if (selected_index > 0) { selected_index--; sync(); }
                } else if (event.keyboard.key == txui::Key::Down) {
                    selected_index++; sync();
                } else if (event.keyboard.key == txui::Key::Tab) {
                    selected_index = (selected_index + 1) % std::max(current_results.size(), size_t{1});
                    sync();
                } else if (event.keyboard.key == txui::Key::Backspace) {
                    if (!query.empty()) { query.pop_back(); selected_index = 0; sync(); }
                } else if (ctrl && event.keyboard.key >= txui::Key::N1 &&
                           event.keyboard.key <= txui::Key::N9) {
                    size_t j = static_cast<size_t>(
                        static_cast<int>(event.keyboard.key) - static_cast<int>(txui::Key::N1));
                    if (j < current_results.size()) { spawn_app(current_results[j]); running = false; }
                } else {
                    char ch = key_to_char(event.keyboard.key, shift);
                    if (ch != '\0') { query += ch; selected_index = 0; sync(); }
                }
            }
        }

        if (!running) break;

        if (needs_redraw) {
            window->present();
            needs_redraw = false;
        }

        window->wait_timeout(animating ? 16 : 100);
    }
    log::info("[Launcher] Exiting cleanly.");
    return 0;
}
