// tinexus-shell — Aura + Pulse Unified Shell
// Architecture:
//   Aura  = always-visible top-center pill (time/wifi/avatar/battery)
//   Pulse = Spotlight-style centered overlay triggered by Ctrl+K
//
// Two-surface architecture:
//   aura_window  : layer=TOP, anchor=TOP|LEFT|RIGHT, centered via margin, KEYBOARD_INTERACTIVITY_NONE
//   pulse_window : layer=TOP (same surface, but height > 100px → compositor grants focus via heuristic)
//
// Focus contract:
//   Idle:   aura has no keyboard focus. Global shortcuts handled by compositor ShortcutEngine.
//   Active: Ctrl+K → shell resizes to fullscreen → compositor sees height > 100 → grants exclusive focus.
//           Escape → shell resizes to 360×48 → compositor clears focus, restores previous window.

#include <txui/window/Window.hpp>
#include <txui/widgets/SolidColorWidget.hpp>
#include <txui/widgets/SizedBox.hpp>
#include <txui/widgets/TextWidget.hpp>
#include <txui/layout/FlexLayout.hpp>
#include <txui/theme/Theme.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <common/logger.hpp>
#include <common/dbus_power.hpp>
#include <indexer/desktop_entry.hpp>
#include <unistd.h>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <atomic>

using namespace tinexus;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Data types
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

static std::vector<AppItem> g_recent_launches;
constexpr size_t MAX_RECENT = 5;

#include <filesystem>
#include <fstream>
#include <cctype>
#include <fcntl.h>
#include "ipcd/protocol/header.hpp"

// ---------------------------------------------------------------------------
// is_process_running
// ---------------------------------------------------------------------------
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
            if (name == comm_name) {
                return true;
            }
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// spawn_app
// ---------------------------------------------------------------------------
static pid_t spawn_app(const AppItem& item) {
    if (item.kind == ResultKind::System) {
        const std::string& cmd = item.exec;
        if (cmd == "lock") {
            log::info("[Pulse] System action: Lock Screen");
            pid_t pid = fork();
            if (pid == 0) { setsid(); execlp("tinexus-lock", "tinexus-lock", nullptr); _exit(127); }
            return pid;
        } else if (cmd == "shutdown") { tinexus::common::dbus_power::poweroff(); return -1; }
        else if (cmd == "reboot")   { tinexus::common::dbus_power::reboot();   return -1; }
        else if (cmd == "sleep")    { tinexus::common::dbus_power::suspend();  return -1; }
        else if (cmd == "logout")   { tinexus::common::dbus_power::logout(); return -1; }
        return -1;
    }
    std::string clean_exec = indexer::DesktopParser::sanitize_exec(item.exec);
    if (clean_exec.empty()) return -1;

    // Single-instance handling for settings and monitor
    if (clean_exec == "tinexus-settings-ui" || clean_exec == "tinexus-monitor") {
        if (is_process_running(clean_exec)) {
            log::info("[Pulse] App {} is already running — sending focus request to compositor", clean_exec);
            const char* xdg_runtime = getenv("XDG_RUNTIME_DIR");
            if (xdg_runtime) {
                std::string fifo_path = std::string(xdg_runtime) + "/tinexus_comp_cmd";
                int fd = open(fifo_path.c_str(), O_WRONLY | O_NONBLOCK);
                if (fd >= 0) {
                    std::string app_id = (clean_exec == "tinexus-settings-ui") ? "tinexus-settings" : clean_exec;
                    std::string cmd = "focus " + app_id + "\n";
                    write(fd, cmd.c_str(), cmd.size());
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
    if (pid < 0) { log::error("[Pulse] fork() failed"); return -1; }
    if (pid == 0) {
        setsid();
        if (item.is_terminal) {
            execlp("foot", "foot", "-e", clean_exec.c_str(), nullptr);
            execlp("weston-terminal", "weston-terminal", nullptr);
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
    log::info("[Pulse] Spawned '{}' PID={}", clean_exec, pid);
    return pid;
}

// ---------------------------------------------------------------------------
// Calculator
// ---------------------------------------------------------------------------
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

static char key_to_char(txui::Key key, bool shift) {
    if (key >= txui::Key::A && key <= txui::Key::Z) {
        char c = static_cast<char>('a' + (static_cast<int>(key) - static_cast<int>(txui::Key::A)));
        if (shift) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return c;
    }
    if (key >= txui::Key::N0 && key <= txui::Key::N9)
        return static_cast<char>('0' + (static_cast<int>(key) - static_cast<int>(txui::Key::N0)));
    if (key == txui::Key::Space) return ' ';
    return '\0';
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

static std::vector<AppItem> load_system_apps() {
    std::vector<AppItem> apps;
    apps.push_back({"Tinexus Terminal", "tinexus-terminal", "Default Wayland Terminal", false, ""});
    apps.push_back({"Foot Terminal", "foot", "Fast Wayland Terminal", true, ""});
    apps.push_back({"Weston Terminal", "weston-terminal", "Wayland Demo Terminal", true, ""});
    apps.push_back({"Alacritty", "alacritty", "GPU Accelerated Terminal", true, ""});
    apps.push_back({"Tinexus System Monitor", "tinexus-monitor", "Resource & Process Monitor", false, ""});
    apps.push_back({"Tinexus Settings", "tinexus-settings-ui", "System Configuration", false, ""});
    apps.push_back({"Tinexus Package Manager", "tinexus-pkg", "Software Manager", false, ""});
    apps.push_back({"Tinexus Files", "tinexus-files", "File Manager", false, ""});
    apps.push_back({"Tinexus App Installer", "tinexus-app-installer", "Install .txapp packages", false, ""});

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

static std::vector<AppItem> build_results(const std::string& q, const std::vector<AppItem>& all) {
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

// ─────────────────────────────────────────────────────────────────────────────
// Design tokens
// ─────────────────────────────────────────────────────────────────────────────
#include <txui/widgets/Widget.hpp>

namespace aura_ui {
    // Premium warm-neutral dark palette (Apple HIG dark mode reference)
    constexpr txui::Color AURA_BG    { 28,  28,  30, 220}; // #1c1c1e @86% — frosted glass feel
    constexpr txui::Color AURA_BG2   { 35,  35,  38, 220}; // slightly lighter gradient stop
    constexpr txui::Color BORDER_TOP {255, 255, 255,  20}; // glass top-edge highlight
    constexpr txui::Color BORDER_RIM {255, 255, 255,   8}; // sides/bottom subtle rim
    constexpr txui::Color SHADOW_1   {  0,   0,   0,  40}; // closest drop shadow
    constexpr txui::Color SHADOW_2   {  0,   0,   0,  25}; // mid shadow
    constexpr txui::Color SHADOW_3   {  0,   0,   0,  15}; // furthest shadow
    constexpr txui::Color TXT_PRI    {240, 240, 248, 255};
    constexpr txui::Color WIFI_COL   {100, 210, 100, 220};
    constexpr txui::Color ACCENT     {107, 140, 239, 255};
}

namespace pulse_ui {
    constexpr txui::Color SCRIM      {  0,   0,   0, 150}; // fullscreen dim
    constexpr txui::Color BG_T       { 28,  28,  30, 248}; // card gradient top
    constexpr txui::Color BG_B       { 22,  22,  25, 248}; // card gradient bottom
    constexpr txui::Color ACCENT     {107, 140, 239, 255};
    constexpr txui::Color SEL_APP    { 59, 130, 246, 200};
    constexpr txui::Color SEL_SYS    {239,  68,  68, 190};
    constexpr txui::Color SEL_CALC   { 16, 185, 129, 200};
    constexpr txui::Color TXT_PRI    {240, 240, 248, 255};
    constexpr txui::Color TXT_SEC    {180, 180, 210, 200};
    constexpr txui::Color TXT_DIM    {120, 120, 150, 150};
    constexpr txui::Color BORDER_TOP {255, 255, 255,  20};
    constexpr txui::Color SHADOW_1   {  0,   0,   0,  40};
    constexpr txui::Color SHADOW_2   {  0,   0,   0,  25};
    constexpr txui::Color SHADOW_3   {  0,   0,   0,  15};
    constexpr double CARD_W    = 680.0;
    constexpr double PANEL_RAD = 20.0;
    constexpr double ROW_H     = 46.0;
    constexpr double ROW_GAP   =  2.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// AuraWidget — top-center pill display
// ─────────────────────────────────────────────────────────────────────────────
class AuraWidget : public txui::Widget {
public:
    txui::Size measure_override(const txui::Constraints& c) noexcept override {
        return txui::Size(c.max_width, c.max_height);
    }

    void paint_override(txui::Painter& painter) const noexcept override {
        using namespace aura_ui;
        const auto& f = frame();
        const double W  = f.width(), H = f.height();
        const double cx = f.x() + W / 2.0, cy = f.y() + H / 2.0;
        const double R  = H / 2.0; // full pill radius — stadium shape

        // ── Fake elevation shadows (Optimized: single pass) ───────────────
        painter.fill_rounded_rect({f.x() - 2, f.y() + 8, W + 4, H}, static_cast<int>(R), SHADOW_2);

        // ── Pill body ─────────────────────────────────────────────────
        painter.fill_rounded_rect(f, static_cast<int>(R), AURA_BG);

        constexpr double PAD_H = 18.0;

        // ── LEFT: time + wifi bars ────────────────────────────────────
        time_t now = time(nullptr);
        struct tm tb;
        localtime_r(&now, &tb);
        char ts[16];
        strftime(ts, sizeof(ts), "%H:%M", &tb);
        // Center text vertically by shifting y up by half the font size (15 / 2 = 7.5)
        painter.draw_text({f.x() + PAD_H, cy - 7.5}, ts, TXT_PRI, 15);

        double wx = f.x() + PAD_H + 54.0;
        for (int b = 0; b < 3; ++b) {
            double bh = 6.0 + static_cast<double>(b) * 3.0; // Heights: 6, 9, 12
            // Vertically center the bars relative to cy. Center of a bar is cy, so top is cy - bh/2.
            painter.fill_rounded_rect({wx + static_cast<double>(b) * 5.0, cy - bh / 2.0, 3.0, bh},
                                       1, WIFI_COL);
        }

        // ── CENTER: avatar circle ─────────────────────────────────────
        const double r = H / 2.0 - 6.0;
        painter.fill_rounded_rect({cx - r, f.y() + 6.0, r * 2.0, r * 2.0},
                                   static_cast<int>(r), ACCENT);
        // Center "T" text vertically and horizontally. Text size is 15.
        painter.draw_text({cx - 5.0, cy - 7.5}, "T", txui::Color(255, 255, 255, 240), 15);

        // ── RIGHT: battery ────────────────────────────────────────────
        constexpr double PCT = 85.0;
        const txui::Color bc = PCT > 20.0 ? txui::Color{100, 220, 130, 220} : txui::Color{240, 80, 80, 220};
        const double bx = f.x() + W - PAD_H - 58.0, by = cy - 7.0; // Height is 14, so by = cy - 7 means centered!
        painter.fill_rounded_rect({bx, by, 22.0, 14.0}, 2, txui::Color{60, 60, 80, 200});
        painter.fill_rounded_rect({bx + 22.0, by + 3.5, 3.0, 7.0}, 1, txui::Color{60, 60, 80, 200});
        painter.fill_rounded_rect({bx + 2.0, by + 2.0, 18.0 * PCT / 100.0, 10.0}, 1, bc);
        char ps[8]; snprintf(ps, sizeof(ps), "%.0f%%", PCT);
        // Center text vertically by shifting y up by half the font size (13 / 2 = 6.5)
        painter.draw_text({bx + 28.0, cy - 6.5}, ps, TXT_PRI, 13);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PulseWidget — fullscreen overlay: scrim + centered search card
// ─────────────────────────────────────────────────────────────────────────────
class PulseWidget : public txui::Widget {
public:
    int                  hovered_index{-1};
    txui::Point          mouse_pos{-100, -100};
    std::string          query;
    std::vector<AppItem> results;
    size_t               selected_index{0};
    bool                 is_launching{false};
    AppItem              launch_app;

    txui::Size measure_override(const txui::Constraints& c) noexcept override {
        return txui::Size(c.max_width, c.max_height);
    }

    void paint_override(txui::Painter& painter) const noexcept override {
        using namespace pulse_ui;
        const double SW = frame().width(), SH = frame().height();

        // Must clear wayland SHM buffer fully before drawing transparent scrim
        painter.clear(txui::Color(0, 0, 0, 0));

        // ── 1. Fullscreen scrim removed for performance ───────────────
        if (is_launching) {
            const double cx = SW * 0.5, cy = SH * 0.5;
            const double cw = 400.0, ch = 240.0;
            const double px = cx - cw * 0.5, py = cy - ch * 0.5;
            painter.fill_gradient_rounded_rect(txui::Rect(px, py, cw, ch), PANEL_RAD, BG_T, BG_B);
            txui::Color cat = launch_app.kind == ResultKind::Calculator ? SEL_CALC :
                              launch_app.kind == ResultKind::System ? SEL_SYS : SEL_APP;
            painter.fill_circle(txui::Point(cx, cy - 20), 40.0, txui::Color(cat.r(), cat.g(), cat.b(), 60));
            painter.draw_circle(txui::Point(cx, cy - 20), 40.0, 2.0, cat);
            painter.draw_text(txui::Point(cx - static_cast<double>(launch_app.name.size()) * 4.5, cy + 40),
                              launch_app.name, TXT_PRI, 1.5);
            return;
        }

        // ── 2. Centered search card ───────────────────────────────────
        const double SEARCH_H = 58.0;
        const double rows_n   = static_cast<double>(std::min(results.size(), size_t{7}));
        const double card_h   = SEARCH_H + rows_n * (ROW_H + ROW_GAP) + 20.0;
        const double px = (SW - CARD_W) * 0.5;
        const double py = SH * 0.38 - card_h * 0.5; // slightly above center — Spotlight style

        // Fake elevation shadows behind card (Optimized: single pass)
        painter.fill_rounded_rect({px - 4, py + 10, CARD_W + 8, card_h}, static_cast<int>(PANEL_RAD), SHADOW_2);

        // Card body
        painter.fill_gradient_rounded_rect(txui::Rect(px, py, CARD_W, card_h), PANEL_RAD, BG_T, BG_B);

        // Glass border
        painter.fill_rounded_rect({px + 3, py + 1, CARD_W - 6, 1}, 0, BORDER_TOP);
        painter.fill_rounded_rect({px - 1, py - 1, CARD_W + 2, card_h + 2},
                                   static_cast<int>(PANEL_RAD) + 1, txui::Color(255, 255, 255, 8));

        // ── 3. Search row ────────────────────────────────────────────
        const double srch_y = py;
        painter.fill_gradient_rect(txui::Rect(px, srch_y, CARD_W, SEARCH_H),
                                   txui::Color(32, 32, 36, 255), txui::Color(26, 26, 30, 255));
        painter.draw_circle(txui::Point(px + 34, srch_y + SEARCH_H * 0.5), 10.0, 1.5, ACCENT);
        painter.fill_gradient_rounded_rect(
            txui::Rect(px + 40, srch_y + SEARCH_H * 0.5 + 5.0, 9.0, 2.0), 1.0,
            ACCENT, txui::Color(80, 110, 200, 160));
        const std::string disp = query.empty() ? "Search apps, run commands..." : (query + "_");
        painter.draw_text(txui::Point(px + 58, srch_y + (SEARCH_H - 16.0) * 0.5),
                          disp, query.empty() ? TXT_DIM : TXT_PRI, 1.0);
        painter.fill_gradient_rect(txui::Rect(px + 14, srch_y + SEARCH_H - 1, CARD_W - 28, 1),
                                   txui::Color(107, 140, 239, 50), txui::Color(107, 140, 239, 0), true);

        // ── 4. Result rows ───────────────────────────────────────────
        if (!results.empty()) {
            const size_t n = std::min(results.size(), size_t{7});
            const double rows_y = srch_y + SEARCH_H + 8.0;
            for (size_t i = 0; i < n; ++i) {
                const auto& item = results[i];
                const bool  sel  = (i == selected_index);
                const double ry  = rows_y + static_cast<double>(i) * (ROW_H + ROW_GAP);
                txui::Color cat  = item.kind == ResultKind::Calculator ? SEL_CALC :
                                   item.kind == ResultKind::System     ? SEL_SYS  : SEL_APP;
                if (sel) {
                    painter.fill_gradient_rounded_rect(txui::Rect(px + 8, ry + 1, CARD_W - 16, ROW_H - 2), 10.0,
                        txui::Color(cat.r(), cat.g(), cat.b(), 52), txui::Color(cat.r(), cat.g(), cat.b(), 22));
                    painter.fill_gradient_rounded_rect(txui::Rect(px + 10, ry + 8, 3, ROW_H - 16), 1.5,
                        cat, txui::Color(cat.r(), cat.g(), cat.b(), 110));
                } else if (static_cast<int>(i) == hovered_index) {
                    painter.fill_gradient_rounded_rect(txui::Rect(px + 8, ry + 1, CARD_W - 16, ROW_H - 2), 10.0,
                        txui::Color(255, 255, 255, 15), txui::Color(255, 255, 255, 5));
                }
                const double icx = px + 34.0, icy = ry + ROW_H * 0.5;
                if (sel) {
                    painter.fill_circle(txui::Point(icx, icy), 13.0, txui::Color(cat.r(), cat.g(), cat.b(), 45));
                    painter.draw_circle(txui::Point(icx, icy), 13.0, 1.0, txui::Color(cat.r(), cat.g(), cat.b(), 150));
                } else {
                    painter.draw_circle(txui::Point(icx, icy), 11.0, 1.0, txui::Color(80, 80, 130, 70));
                }
                painter.fill_circle(txui::Point(icx, icy), 3.5, sel ? cat : txui::Color(110, 110, 160, 130));
                painter.draw_text(txui::Point(px + 56, ry + (ROW_H - 16.0) * 0.5),
                                  item.name, sel ? TXT_PRI : TXT_SEC, 1.0);
                if (!item.description.empty()) {
                    const size_t md = 30;
                    std::string d = item.description.size() > md
                        ? item.description.substr(0, md) + "..." : item.description;
                    const double dx = px + CARD_W - static_cast<double>(d.size()) * 8.0 - 34.0;
                    if (dx > px + CARD_W * 0.55)
                        painter.draw_text(txui::Point(dx, ry + (ROW_H - 14.0) * 0.5), d, TXT_DIM, 0.9);
                }
                if (i < 9)
                    painter.draw_text(txui::Point(px + CARD_W - 22, ry + (ROW_H - 16.0) * 0.5),
                                      std::to_string(i + 1), TXT_DIM, 1.0);
            }
        }

        // ── 5. Hint row ──────────────────────────────────────────────
        const double tip_y = py + card_h + 10.0;
        const char* tips[] = {"navigate", "launch", "Esc: close", nullptr};
        const char* keys[] = {"Up/Down: ", "Enter: ", "", nullptr};
        double tip_x = px + (CARD_W * 0.5) - 180.0;
        for (int ti = 0; tips[ti]; ++ti) {
            std::string s = std::string(keys[ti]) + std::string(tips[ti]);
            painter.draw_text(txui::Point(tip_x, tip_y), s, txui::Color(120, 120, 155, 100), 1.0);
            tip_x += static_cast<double>(s.size()) * 8.0 + 20.0;
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// IPC listener thread — receives LAUNCHER_SHOW from compositor via ipcd
// ─────────────────────────────────────────────────────────────────────────────
static std::atomic<bool> g_toggle_pulse{false};

void ipc_listener_thread() {
    int fd = -1;
    for (int a = 0; a < 30 && fd < 0; ++a) {
        fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) { ::sleep(1); continue; }
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        char path[108];
        snprintf(path, sizeof(path), "/run/user/%d/tinexus/ipc.sock", static_cast<int>(getuid()));
        memcpy(addr.sun_path, path, strlen(path) + 1);
        if (connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0) {
            close(fd); fd = -1; ::sleep(1);
        }
    }
    if (fd < 0) { log::warn("[Shell] Could not connect to ipcd"); return; }

#pragma pack(push, 1)
    struct Hdr {
        uint32_t magic = 0x544E5853; uint16_t version = 0x0100; uint16_t msg_type;
        uint16_t flags = 0; uint32_t seq = 0; uint32_t payload_len; uint32_t csum = 0;
    };
#pragma pack(pop)
    static_assert(sizeof(Hdr) == 22, "");

    uint16_t topic = 1000;
    Hdr sh; sh.msg_type = static_cast<uint16_t>(tinexus::ipcd::protocol::MessageType::SYS_SUBSCRIBE_TOPIC); sh.payload_len = sizeof(topic);
    send(fd, &sh, sizeof(sh), MSG_NOSIGNAL);
    send(fd, &topic, sizeof(topic), MSG_NOSIGNAL);
    log::info("[Shell] Subscribed to LAUNCHER_OPEN (1000) via ipcd");

    while (true) {
        Hdr rx; ssize_t got = 0;
        auto* raw = reinterpret_cast<uint8_t*>(&rx);
        while (got < static_cast<ssize_t>(sizeof(rx))) {
            ssize_t n = recv(fd, raw + got, sizeof(rx) - static_cast<size_t>(got), 0);
            if (n <= 0) goto done;
            got += n;
        }
        if (rx.payload_len > 0) {
            std::vector<uint8_t> buf(rx.payload_len); ssize_t pg = 0;
            while (pg < static_cast<ssize_t>(rx.payload_len)) {
                ssize_t n = recv(fd, buf.data() + pg, rx.payload_len - static_cast<size_t>(pg), 0);
                if (n <= 0) goto done;
                pg += n;
            }
        }
        if (rx.msg_type == 1004) {
            g_toggle_pulse.store(true);
            log::info("[Shell] LAUNCHER_SHOW received — toggling Pulse");
        }
    }
done:
    close(fd);
    log::warn("[Shell] ipcd connection lost");
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    (void)argc; (void)argv;
    log::set_component_name("shell");
    log::info("[Shell] Tinexus Unified Shell starting...");
    signal(SIGCHLD, SIG_IGN);

    // Aura window — top-center pill, KEYBOARD_INTERACTIVITY_NONE, always visible
    auto window = txui::Window::create(360, 48, "Aura", /*layer_shell=*/true);
    std::thread(ipc_listener_thread).detach();

    if (!window || !window->is_wayland_connected()) {
        log::error("[Shell] Failed to connect to Wayland display! Exiting.");
        return 1;
    }

    std::vector<AppItem> all_apps = load_system_apps();
    std::string query;
    size_t selected_index = 0;
    std::vector<AppItem> current_results;

    auto aura_widget  = txui::make_ref<AuraWidget>();
    auto pulse_widget = txui::make_ref<PulseWidget>();

    // Initially, Aura is the root widget (idle pill mode)
    window->set_root_widget(txui::Ref<txui::Widget>(aura_widget.get()));

    auto sync_pulse = [&]() {
        current_results = build_results(query, all_apps);
        if (current_results.empty()) selected_index = 0;
        else if (selected_index >= current_results.size()) selected_index = current_results.size() - 1;
        pulse_widget->query          = query;
        pulse_widget->selected_index = selected_index;
        pulse_widget->results        = current_results;
        pulse_widget->mark_needs_paint();
    };
    sync_pulse();
    aura_widget->mark_needs_paint();
    window->present();

    bool running = true;
    bool pulse_active = false;
    bool needs_redraw = false;

    // Aura size state (for idle pill)
    constexpr double AURA_W = 360.0, AURA_H = 48.0;
    // Pulse size state (floating search card)
    constexpr double PULSE_W = 720.0, PULSE_H = 540.0;

    double current_w = AURA_W, current_h = AURA_H;
    double target_w = AURA_W, target_h = AURA_H;
    bool animating = false;

    // Launch Fake Flip state
    bool launch_animating = false;
    bool launch_flipped   = false;

    while (running && !window->should_close()) {
        // ── Handle Ctrl+K toggle ─────────────────────────────────────────
        if (g_toggle_pulse.exchange(false)) {
            pulse_active = !pulse_active;
            if (pulse_active) {
                log::info("[Shell] Expanding to Pulse mode");
                query = ""; selected_index = 0;
                sync_pulse();
                // Swap root widget to Pulse (fullscreen overlay with scrim)
                window->set_root_widget(txui::Ref<txui::Widget>(pulse_widget.get()));
                target_w = PULSE_W; target_h = PULSE_H;
                window->set_keyboard_interactivity(true);
            } else {
                log::info("[Shell] Collapsing to Aura pill mode");
                window->set_root_widget(txui::Ref<txui::Widget>(aura_widget.get()));
                target_w = AURA_W; target_h = AURA_H;
                window->set_keyboard_interactivity(false);
            }
            animating = true;
            needs_redraw = true;
        }

        // ── Animation tick ───────────────────────────────────────────────
        if (animating) {
            double dw = target_w - current_w;
            double dh = target_h - current_h;
            
            // Ease-out tuning: fast start, gentle stop (premium feel)
            // Increased multiplier for faster start, but check for small delta for stop
            current_w += dw * 0.4;
            current_h += dh * 0.4;
            if (std::abs(dw) < 1.0 && std::abs(dh) < 1.0) {
                current_w = target_w; current_h = target_h;
                if (!launch_animating) animating = false;
            }
            if (launch_animating) {
                if (!launch_flipped && current_w <= 6.0) {
                    launch_flipped = true;
                    pulse_widget->is_launching = true;
                    target_w = 400.0; target_h = 240.0;
                } else if (launch_flipped && std::abs(dw) < 2.0 && std::abs(dh) < 2.0) {
                    spawn_app(pulse_widget->launch_app);
                    // Smoothly collapse instead of exiting
                    launch_animating = false;
                    launch_flipped = false;
                    pulse_widget->is_launching = false;
                    pulse_active = false;
                    query = ""; selected_index = 0;
                    sync_pulse();
                    window->set_root_widget(txui::Ref<txui::Widget>(aura_widget.get()));
                    target_w = AURA_W; target_h = AURA_H;
                    window->set_keyboard_interactivity(false);
                    animating = true;
                }
            }
            if (current_w > 0.0 && current_h > 0.0) {
                window->resize(static_cast<uint32_t>(current_w), static_cast<uint32_t>(current_h));
                needs_redraw = true;
            }
        }

        // ── Poll events ──────────────────────────────────────────────────
        txui::Event event;
        while (window->poll_event(event)) {
            if (event.type == txui::EventType::WindowClose) {
                running = false;
            } else if (event.type == txui::EventType::PointerMove) {
                pulse_widget->mouse_pos = txui::Point(event.pointer.x, event.pointer.y);
                if (pulse_active && !launch_animating) {
                    const double SW = PULSE_W;
                    const double SH = PULSE_H;
                    const double SEARCH_H = 58.0;
                    const double ROW_H = 44.0;
                    const double ROW_GAP = 8.0;
                    const double rows_n = static_cast<double>(std::min(pulse_widget->results.size(), size_t{7}));
                    const double card_h = SEARCH_H + rows_n * (ROW_H + ROW_GAP) + 20.0;
                    const double px = (SW - 680.0) * 0.5;
                    const double py = SH * 0.38 - card_h * 0.5;
                    const double rows_y = py + SEARCH_H + 8.0;
                    
                    int new_hover = -1;
                    if (event.pointer.x >= px && event.pointer.x <= px + 680.0 && event.pointer.y >= rows_y && event.pointer.y <= rows_y + rows_n * (ROW_H + ROW_GAP)) {
                        new_hover = static_cast<int>((event.pointer.y - rows_y) / (ROW_H + ROW_GAP));
                    }
                    if (pulse_widget->hovered_index != new_hover) {
                        pulse_widget->hovered_index = new_hover;
                        needs_redraw = true;
                    }
                }
            } else if (event.type == txui::EventType::PointerButtonPress && !launch_animating) {
                if (event.pointer.button == txui::MouseButton::Left && pulse_active) {
                    const double SW = PULSE_W;
                    const double SH = PULSE_H;
                    const double SEARCH_H = 58.0;
                    const double ROW_H = 44.0;
                    const double ROW_GAP = 8.0;
                    const double rows_n = static_cast<double>(std::min(pulse_widget->results.size(), size_t{7}));
                    const double card_h = SEARCH_H + rows_n * (ROW_H + ROW_GAP) + 20.0;
                    const double px = (SW - 680.0) * 0.5;
                    const double py = SH * 0.38 - card_h * 0.5;
                    
                    if (event.pointer.x < px || event.pointer.x > px + 680.0 || event.pointer.y < py || event.pointer.y > py + card_h) {
                        pulse_active = false;
                        query = ""; selected_index = 0; sync_pulse();
                        window->set_root_widget(txui::Ref<txui::Widget>(aura_widget.get()));
                        target_w = AURA_W; target_h = AURA_H;
                        window->set_keyboard_interactivity(false);
                        animating = true;
                        needs_redraw = true;
                    } else if (pulse_widget->hovered_index >= 0 && pulse_widget->hovered_index < static_cast<int>(pulse_widget->results.size())) {
                        pulse_widget->launch_app = pulse_widget->results[static_cast<size_t>(pulse_widget->hovered_index)];
                        launch_animating = true; animating = true; target_w = 2.0;
                        needs_redraw = true;
                    }
                }
            } else if (event.type == txui::EventType::KeyDown && !launch_animating) {
                needs_redraw = true;
                const bool shift = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift);
                const bool ctrl  = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Ctrl);

                if (pulse_active) {
                    if (event.keyboard.key == txui::Key::Escape) {
                        pulse_active = false;
                        query = ""; selected_index = 0;
                        window->set_root_widget(txui::Ref<txui::Widget>(aura_widget.get()));
                        target_w = AURA_W; target_h = AURA_H;
                        window->set_keyboard_interactivity(false);
                        animating = true;
                    } else if (event.keyboard.key == txui::Key::Enter) {
                        if (!current_results.empty() && selected_index < current_results.size()) {
                            pulse_widget->launch_app = current_results[selected_index];
                            launch_animating = true; animating = true; target_w = 2.0;
                        } else if (!query.empty()) {
                            AppItem ci; ci.name = query; ci.exec = query; ci.kind = ResultKind::App;
                            pulse_widget->launch_app = ci;
                            launch_animating = true; animating = true; target_w = 2.0;
                        }
                    } else if (event.keyboard.key == txui::Key::Up) {
                        if (selected_index > 0) { selected_index--; sync_pulse(); }
                    } else if (event.keyboard.key == txui::Key::Down) {
                        selected_index++; sync_pulse();
                    } else if (event.keyboard.key == txui::Key::Tab) {
                        selected_index = (selected_index + 1) % std::max(current_results.size(), size_t{1});
                        sync_pulse();
                    } else if (event.keyboard.key == txui::Key::Backspace) {
                        if (!query.empty()) { query.pop_back(); selected_index = 0; sync_pulse(); }
                    } else if (ctrl && event.keyboard.key >= txui::Key::N1 && event.keyboard.key <= txui::Key::N9) {
                        size_t j = static_cast<size_t>(static_cast<int>(event.keyboard.key) - static_cast<int>(txui::Key::N1));
                        if (j < current_results.size()) {
                            spawn_app(current_results[j]);
                            pulse_active = false;
                            query = ""; selected_index = 0; sync_pulse();
                            window->set_root_widget(txui::Ref<txui::Widget>(aura_widget.get()));
                            target_w = AURA_W; target_h = AURA_H; 
                            window->set_keyboard_interactivity(false);
                            animating = true;
                        }
                    } else {
                        char ch = key_to_char(event.keyboard.key, shift);
                        if (ch != '\0') { query += ch; selected_index = 0; sync_pulse(); }
                    }
                }
            }
        }

        if (!running) break;

        // ── Render ───────────────────────────────────────────────────────
        if (needs_redraw) {
            window->present();
            needs_redraw = false;
        }

        window->wait_timeout(animating ? 16 : 100);
    }

    log::info("[Shell] Exiting cleanly.");
    return 0;
}
