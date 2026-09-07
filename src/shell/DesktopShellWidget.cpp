#include "DesktopShellWidget.hpp"
#include <common/TinexusLogo.hpp>
#include <common/AudioUtils.hpp>
#include <common/BacklightUtils.hpp>
#include <txui/render/FontMetrics.hpp>
#include <unistd.h>
#include <sys/reboot.h>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace tinexus::shell {

namespace fs = std::filesystem;

namespace ui {
    constexpr txui::Color BAR_BG        { 13,  14,  18, 245};
    constexpr txui::Color BORDER_LINE   {255, 255, 255,  15};
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

std::vector<AppItem> load_system_apps() {
    std::vector<AppItem> apps;
    const std::vector<std::string> dirs = {
        "/usr/share/applications",
        "/usr/local/share/applications"
    };

    for (const auto& d : dirs) {
        if (!fs::exists(d)) continue;
        try {
            for (const auto& entry : fs::directory_iterator(d)) {
                if (!entry.is_regular_file() || entry.path().extension() != ".desktop") continue;
                auto p = tinexus::indexer::DesktopParser::parse_file(entry.path());
                if (!p || p->no_display || p->name.empty() || p->exec.empty()) continue;
                bool dup = std::any_of(apps.begin(), apps.end(), [&](const AppItem& a) {
                    return a.name == p->name || a.exec == p->exec;
                });
                if (!dup) {
                    AppItem item;
                    item.name = p->name;
                    item.exec = p->exec;
                    item.description = p->comment.empty() ? p->generic_name : p->comment;
                    item.is_terminal = p->terminal;
                    item.icon = p->icon;
                    item.kind = ResultKind::App;
                    apps.push_back(std::move(item));
                }
            }
        } catch (...) {}
    }
    return apps;
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
        }
    }

    log::info("[Shell] Spawning application: {}", item.name);
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        std::string exe = item.exec;
        auto pos = exe.find('%');
        if (pos != std::string::npos) exe = exe.substr(0, pos);
        while (!exe.empty() && exe.back() == ' ') exe.pop_back();

        if (item.is_terminal) {
            execlp("tinexus-terminal", "tinexus-terminal", "-e", exe.c_str(), nullptr);
            execlp("foot", "foot", "-e", exe.c_str(), nullptr);
        } else {
            execlp(exe.c_str(), exe.c_str(), nullptr);
        }
        _exit(127);
    }
    return pid;
}

double read_battery_percent() {
    if (!fs::exists("/sys/class/power_supply")) {
        return -1.0;
    }

    double total_cap = 0.0;
    int battery_count = 0;

    try {
        for (const auto& entry : fs::directory_iterator("/sys/class/power_supply")) {
            if (!entry.is_directory()) continue;

            // Check supply type: must be "Battery"
            std::string type;
            fs::path type_file = entry.path() / "type";
            if (fs::exists(type_file)) {
                std::ifstream tf(type_file);
                tf >> type;
            }

            // Also check name for BAT* prefix as a reliable fallback
            std::string name = entry.path().filename().string();
            bool is_battery = (type == "Battery" || name.rfind("BAT", 0) == 0 || name.find("battery") != std::string::npos);
            if (!is_battery) continue;

            // Check if battery is present
            fs::path present_file = entry.path() / "present";
            if (fs::exists(present_file)) {
                std::ifstream pf(present_file);
                int present = 1;
                if (pf >> present && present == 0) continue;
            }

            // Read capacity
            fs::path cap_file = entry.path() / "capacity";
            if (fs::exists(cap_file)) {
                std::ifstream cf(cap_file);
                int cap = -1;
                if (cf >> cap && cap >= 0 && cap <= 100) {
                    total_cap += cap;
                    battery_count++;
                }
            }
        }
    } catch (...) {
        return -1.0;
    }

    if (battery_count > 0) {
        return total_cap / battery_count;
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

    s_cached_bars = bars;
    s_cached_conn = connected;
    out_bars = bars;
    out_connected = connected;
}

DesktopShellWidget::DesktopShellWidget() {
    all_apps = load_system_apps();

    m_notch = txui::make_ref<AuraNotchWidget>();
    m_logo_menu = txui::make_ref<LogoMenuWidget>();
    m_calendar_flyout = txui::make_ref<CalendarFlyoutWidget>();
    m_notification_flyout = txui::make_ref<NotificationFlyoutWidget>();
    m_volume_flyout = txui::make_ref<VolumeFlyoutWidget>();
    m_brightness_flyout = txui::make_ref<BrightnessFlyoutWidget>();

    // Notch triggers
    m_notch->on_center_clicked = [this]() {
        close_all_flyouts();
        if (on_pulse_toggle_requested) {
            on_pulse_toggle_requested();
        }
    };

    m_notch->on_date_clicked = [this]() {
        bool was_open = calendar_open;
        close_all_flyouts();
        calendar_open = !was_open;
        m_notch->set_calendar_open(calendar_open);
        if (on_resize_requested) {
            on_resize_requested(calendar_open ? 350.0 : 46.0);
        }
        mark_needs_paint();
    };

    // Logo menu actions
    m_logo_menu->on_action_selected = [this](LogoMenuAction action) {
        close_all_flyouts();
        switch (action) {
            case LogoMenuAction::AboutTinexus:
                spawn_app({"About Tinexus", "tinexus-about", "System Profiler", false, ""});
                break;
            case LogoMenuAction::SystemSettings:
                spawn_app({"Settings", "tinexus-settings-ui", "Settings", false, ""});
                break;
            case LogoMenuAction::AppInstaller:
                spawn_app({"App Installer", "tinexus-app-installer", "Installer", false, ""});
                break;
            case LogoMenuAction::SystemMonitor:
                spawn_app({"System Monitor", "tinexus-monitor", "Monitor", false, ""});
                break;
            case LogoMenuAction::Sleep:
                log::info("[Shell] System Sleep triggered");
                break;
            case LogoMenuAction::Restart:
                spawn_app({"Restart", "reboot", "", false, "", ResultKind::System});
                break;
            case LogoMenuAction::ShutDown:
                spawn_app({"Shut Down", "shutdown", "", false, "", ResultKind::System});
                break;
            case LogoMenuAction::LockScreen:
                spawn_app({"Lock Screen", "lock", "", false, "", ResultKind::System});
                break;
        }
    };

    // Notification flyout clear
    m_notification_flyout->on_clear_all = [this]() {
        notifications.clear();
        mark_needs_paint();
    };

    sync_notifications();
}

void DesktopShellWidget::sync_notifications() {
    if (notifications.empty()) {
        notifications = {
            {1, "Platform Ready", "Tinexus Desktop v1.0", "Wayland Vulkan compositing active on DRM KMS", "Just now", ui::ACCENT_CYAN, 1},
            {2, "Network", "Wi-Fi Connected", "Primary interface active and online", "5m ago", ui::WIFI_COL, 1},
            {3, "Supervisor", "Platform Supervision", "Supervision tree active, daemons sandboxed", "12m ago", ui::ACCENT_BLUE, 0}
        };
    }
    if (m_notification_flyout) {
        m_notification_flyout->set_notifications(notifications);
    }
}

void DesktopShellWidget::close_all_flyouts() {
    bool changed = (logo_menu_open || app_menu_open || calendar_open || notifications_open ||
                    volume_flyout_open || brightness_flyout_open || pulse_active);
    logo_menu_open = false;
    app_menu_open = false;
    calendar_open = false;
    notifications_open = false;
    volume_flyout_open = false;
    brightness_flyout_open = false;
    pulse_active = false;

    if (m_notch) m_notch->set_calendar_open(false);

    if (changed && on_resize_requested) {
        on_resize_requested(46.0);
    }
    mark_needs_paint();
}

txui::Size DesktopShellWidget::measure_override(const txui::Constraints& c) noexcept {
    return txui::Size(c.max_width, c.max_height);
}

void DesktopShellWidget::layout_override(const txui::Rect& f) noexcept {
    const double W = f.width();
    const double cx = f.x() + W * 0.5;

    if (m_notch) {
        m_notch->layout(txui::Rect(cx - 150.0, f.y(), 300.0, 46.0));
    }
    if (m_logo_menu) {
        m_logo_menu->layout(txui::Rect(f.x() + 8.0, f.y() + 38.0, 236.0, 260.0));
    }
    if (m_calendar_flyout) {
        m_calendar_flyout->layout(txui::Rect(cx - 150.0, f.y() + 54.0, 300.0, 285.0));
    }
    if (m_notification_flyout) {
        double nh = m_notification_flyout->calculate_height();
        m_notification_flyout->layout(txui::Rect(f.x() + W - 406.0, f.y() + 44.0, 390.0, nh));
    }
    if (m_volume_flyout) {
        m_volume_flyout->layout(txui::Rect(f.x() + W - 250.0, f.y() + 42.0, 240.0, 140.0));
    }
    if (m_brightness_flyout) {
        m_brightness_flyout->layout(txui::Rect(f.x() + W - 280.0, f.y() + 42.0, 240.0, 135.0));
    }
}

void DesktopShellWidget::paint_override(txui::Painter& painter) const noexcept {
    using namespace ui;
    const auto& f = frame();
    const double W = f.width();
    const double cx = f.x() + W * 0.5;

    // Clear Wayland SHM surface to transparent
    painter.clear(txui::Color(0, 0, 0, 0));

    // ── 1. Full-Width Top Bar (32px high) ──────────────────────────────────
    constexpr double BAR_H = 32.0;
    painter.fill_rect(txui::Rect(f.x(), f.y(), W, BAR_H), BAR_BG);

    // Hairline border along bottom of 32px bar, meeting notch slopes
    constexpr double NOTCH_H = 46.0;
    constexpr double NOTCH_TOP_HALF = 136.0;
    constexpr double NOTCH_BOT_HALF = 98.0;
    constexpr double NOTCH_SLOPE = NOTCH_TOP_HALF - NOTCH_BOT_HALF;
    double t_bar = BAR_H / NOTCH_H;
    double lx_bar = (cx - NOTCH_TOP_HALF) + t_bar * NOTCH_SLOPE;
    double rx_bar = (cx + NOTCH_TOP_HALF) - t_bar * NOTCH_SLOPE;

    painter.draw_line(txui::Point(f.x(), f.y() + BAR_H - 1.0), txui::Point(lx_bar, f.y() + BAR_H - 1.0), 1.0, BORDER_LINE);
    painter.draw_line(txui::Point(rx_bar, f.y() + BAR_H - 1.0), txui::Point(f.x() + W, f.y() + BAR_H - 1.0), 1.0, BORDER_LINE);

    // Left: Tinexus Logo Button
    if (hover_logo || logo_menu_open) {
        painter.fill_rounded_rect(txui::Rect(f.x() + 6.0, f.y() + 3.0, 30.0, 26.0), 6.0, txui::Color(255, 255, 255, 22));
    }
    auto logo_buf = logo::get_logo(32);
    if (logo_buf.is_valid()) {
        painter.draw_image(txui::Rect(f.x() + 10.0, f.y() + 5.0, 22.0, 22.0), logo_buf.pixels, logo_buf.width, logo_buf.height);
    } else {
        painter.fill_circle(txui::Point(f.x() + 21.0, f.y() + 16.0), 10.0, txui::Color(30, 35, 48, 255));
        painter.draw_circle(txui::Point(f.x() + 21.0, f.y() + 16.0), 10.0, 1.2, ACCENT_BLUE);
        painter.draw_circle(txui::Point(f.x() + 21.0, f.y() + 16.0), 7.5, 1.0, ACCENT_CYAN);
        painter.fill_circle(txui::Point(f.x() + 21.0, f.y() + 16.0), 3.5, txui::Color(255, 255, 255, 240));
    }

    // Left: Active Application Title Trigger (measured with FontMetrics)
    double app_text_w = txui::FontMetrics::measure(active_app_name, 13.0).width;
    double app_label_w = app_text_w + 28.0;
    if (hover_app_title || app_menu_open) {
        painter.fill_rounded_rect(txui::Rect(f.x() + 42.0, f.y() + 3.0, app_label_w, 26.0), 5.0, txui::Color(255, 255, 255, 18));
    }
    painter.draw_text(txui::Point(f.x() + 48.0, f.y() + 8.0), active_app_name, TXT_PRI, 13.0, true);
    double chv_x = f.x() + 48.0 + app_text_w + 8.0;
    painter.draw_line(txui::Point(chv_x, f.y() + 14.5), txui::Point(chv_x + 3.5, f.y() + 18.0), 1.4, TXT_SEC);
    painter.draw_line(txui::Point(chv_x + 3.5, f.y() + 18.0), txui::Point(chv_x + 7.0, f.y() + 14.5), 1.4, TXT_SEC);

    // Center: Aura v2 Notch Widget
    if (m_notch) {
        m_notch->paint(painter);
    }

    // Right: Status Tray Icons
    // Sun / Brightness Toggle
    int cur_brightness = hardware::BacklightUtils::get_brightness_percent();
    double sun_x = f.x() + W - 225.0;
    if (hover_sun || brightness_flyout_open) painter.fill_rounded_rect(txui::Rect(sun_x - 4.0, f.y() + 3.0, 26.0, 26.0), 5.0, txui::Color(255, 255, 255, 20));
    painter.draw_circle(txui::Point(sun_x + 9.0, f.y() + 16.0), 4.5, 1.5, TXT_PRI);
    int ray_alpha = std::clamp(static_cast<int>(100 + (cur_brightness * 155) / 100), 100, 255);
    txui::Color ray_col(245, 158, 11, static_cast<uint8_t>(ray_alpha));
    for (int a = 0; a < 8; ++a) {
        double ang = static_cast<double>(a) * (3.14159265 / 4.0);
        double px1 = (sun_x + 9.0) + std::cos(ang) * 6.5;
        double py1 = (f.y() + 16.0) + std::sin(ang) * 6.5;
        double px2 = (sun_x + 9.0) + std::cos(ang) * 8.5;
        double py2 = (f.y() + 16.0) + std::sin(ang) * 8.5;
        painter.draw_line(txui::Point(px1, py1), txui::Point(px2, py2), 1.2, ray_col);
    }

    // Volume
    int cur_volume = hardware::AudioUtils::get_volume_percent();
    bool is_vol_muted = hardware::AudioUtils::is_muted() || (cur_volume == 0);
    double vol_x = f.x() + W - 190.0;
    if (hover_vol || volume_flyout_open) painter.fill_rounded_rect(txui::Rect(vol_x - 4.0, f.y() + 3.0, 28.0, 26.0), 5.0, txui::Color(255, 255, 255, 20));
    painter.fill_rect(txui::Rect(vol_x + 2.0, f.y() + 13.0, 4.0, 6.0), TXT_PRI);
    painter.draw_line(txui::Point(vol_x + 6.0, f.y() + 13.0), txui::Point(vol_x + 11.0, f.y() + 10.0), 1.5, TXT_PRI);
    painter.draw_line(txui::Point(vol_x + 11.0, f.y() + 10.0), txui::Point(vol_x + 11.0, f.y() + 22.0), 1.5, TXT_PRI);
    painter.draw_line(txui::Point(vol_x + 11.0, f.y() + 22.0), txui::Point(vol_x + 6.0, f.y() + 19.0), 1.5, TXT_PRI);
    if (is_vol_muted) {
        painter.draw_line(txui::Point(vol_x + 14.0, f.y() + 12.0), txui::Point(vol_x + 20.0, f.y() + 20.0), 1.5, txui::Color(239, 68, 68, 240));
    } else {
        if (cur_volume > 0) {
            painter.draw_line(txui::Point(vol_x + 14.0, f.y() + 14.0), txui::Point(vol_x + 15.5, f.y() + 16.0), 1.2, ACCENT_CYAN);
            painter.draw_line(txui::Point(vol_x + 15.5, f.y() + 16.0), txui::Point(vol_x + 14.0, f.y() + 18.0), 1.2, ACCENT_CYAN);
        }
        if (cur_volume > 33) {
            painter.draw_line(txui::Point(vol_x + 17.0, f.y() + 12.0), txui::Point(vol_x + 19.0, f.y() + 16.0), 1.2, ACCENT_CYAN);
            painter.draw_line(txui::Point(vol_x + 19.0, f.y() + 16.0), txui::Point(vol_x + 17.0, f.y() + 20.0), 1.2, ACCENT_CYAN);
        }
        if (cur_volume > 66) {
            painter.draw_line(txui::Point(vol_x + 20.5, f.y() + 10.0), txui::Point(vol_x + 22.5, f.y() + 16.0), 1.2, ACCENT_BLUE);
            painter.draw_line(txui::Point(vol_x + 22.5, f.y() + 16.0), txui::Point(vol_x + 20.5, f.y() + 22.0), 1.2, ACCENT_BLUE);
        }
    }

    // Battery
    double bat_x = f.x() + W - 150.0;
    if (hover_bat) painter.fill_rounded_rect(txui::Rect(bat_x - 4.0, f.y() + 3.0, 58.0, 26.0), 5.0, txui::Color(255, 255, 255, 20));
    double pct = read_battery_percent();
    double fill_pct = (pct >= 0.0) ? pct : 100.0;
    txui::Color bat_col = (fill_pct > 20.0) ? txui::Color(74, 222, 128, 255) : txui::Color(239, 68, 68, 255);
    painter.fill_rounded_rect(txui::Rect(bat_x, f.y() + 10.0, 22.0, 12.0), 2.0, txui::Color(55, 60, 75, 220));
    painter.fill_rounded_rect(txui::Rect(bat_x + 22.0, f.y() + 13.0, 2.5, 6.0), 1.0, txui::Color(55, 60, 75, 220));
    painter.fill_rounded_rect(txui::Rect(bat_x + 2.0, f.y() + 12.0, 18.0 * (fill_pct / 100.0), 8.0), 1.0, bat_col);
    char pct_str[16];
    if (pct >= 0.0) snprintf(pct_str, sizeof(pct_str), "%.0f%%", pct);
    else snprintf(pct_str, sizeof(pct_str), "DC");
    painter.draw_text(txui::Point(bat_x + 28.0, f.y() + 9.0), pct_str, TXT_PRI, 11.5);

    // Wi-Fi
    double wifi_x = f.x() + W - 82.0;
    if (hover_wifi) painter.fill_rounded_rect(txui::Rect(wifi_x - 6.0, f.y() + 3.0, 32.0, 26.0), 5.0, txui::Color(255, 255, 255, 20));
    int wifi_bars = 0;
    bool has_net = false;
    read_network_status(wifi_bars, has_net);
    txui::Color w_col = has_net ? WIFI_COL : WIFI_DIM;
    painter.fill_circle(txui::Point(wifi_x + 10.0, f.y() + 21.0), 2.0, w_col);
    painter.draw_line(txui::Point(wifi_x + 6.0, f.y() + 18.0), txui::Point(wifi_x + 10.0, f.y() + 15.0), 1.8, (wifi_bars >= 2) ? w_col : WIFI_DIM);
    painter.draw_line(txui::Point(wifi_x + 10.0, f.y() + 15.0), txui::Point(wifi_x + 14.0, f.y() + 18.0), 1.8, (wifi_bars >= 2) ? w_col : WIFI_DIM);
    painter.draw_line(txui::Point(wifi_x + 3.0, f.y() + 15.0), txui::Point(wifi_x + 10.0, f.y() + 11.0), 1.8, (wifi_bars >= 3) ? w_col : WIFI_DIM);
    painter.draw_line(txui::Point(wifi_x + 10.0, f.y() + 11.0), txui::Point(wifi_x + 17.0, f.y() + 15.0), 1.8, (wifi_bars >= 3) ? w_col : WIFI_DIM);
    painter.draw_line(txui::Point(wifi_x + 0.0, f.y() + 12.0), txui::Point(wifi_x + 10.0, f.y() + 7.0), 1.8, (wifi_bars >= 4) ? w_col : WIFI_DIM);
    painter.draw_line(txui::Point(wifi_x + 10.0, f.y() + 7.0), txui::Point(wifi_x + 20.0, f.y() + 12.0), 1.8, (wifi_bars >= 4) ? w_col : WIFI_DIM);

    // Bell / Notifications Button
    double bell_x = f.x() + W - 42.0;
    if (hover_bell || notifications_open) {
        painter.fill_rounded_rect(txui::Rect(bell_x - 4.0, f.y() + 3.0, 28.0, 26.0), 5.0, txui::Color(255, 255, 255, 20));
    }
    painter.draw_line(txui::Point(bell_x + 6.0, f.y() + 18.0), txui::Point(bell_x + 14.0, f.y() + 18.0), 1.5, TXT_PRI);
    painter.draw_line(txui::Point(bell_x + 7.0, f.y() + 18.0), txui::Point(bell_x + 8.5, f.y() + 12.0), 1.5, TXT_PRI);
    painter.draw_line(txui::Point(bell_x + 13.0, f.y() + 18.0), txui::Point(bell_x + 11.5, f.y() + 12.0), 1.5, TXT_PRI);
    painter.draw_circle(txui::Point(bell_x + 10.0, f.y() + 12.0), 2.5, 1.2, TXT_PRI);
    painter.fill_circle(txui::Point(bell_x + 10.0, f.y() + 20.0), 1.5, TXT_PRI);
    if (!notifications.empty()) {
        painter.fill_circle(txui::Point(bell_x + 14.5, f.y() + 10.0), 3.0, txui::Color(249, 115, 22, 255));
    }

    // ── 2. Flyouts Rendering ────────────────────────────────────────────────
    if (logo_menu_open && m_logo_menu) {
        m_logo_menu->paint(painter);
    }

    if (app_menu_open) {
        const double ax = f.x() + 44.0, ay = f.y() + 38.0, aw = 250.0;
        const size_t show_count = std::min(all_apps.size(), size_t{10});
        const double ah = 16.0 + static_cast<double>(show_count) * 28.0;

        painter.fill_rounded_rect(txui::Rect(ax - 4.0, ay + 6.0, aw + 8.0, ah + 4.0), 14.0, txui::Color(0, 0, 0, 95));
        painter.fill_rounded_rect(txui::Rect(ax, ay, aw, ah), 14.0, CARD_BG);
        painter.fill_rounded_rect(txui::Rect(ax - 1.0, ay - 1.0, aw + 2.0, ah + 2.0), 15.0, BORDER_LINE);

        double cur_y = ay + 8.0;
        for (size_t i = 0; i < show_count; ++i) {
            bool sel = (app_menu_hover == static_cast<int>(i));
            if (sel) {
                painter.fill_rounded_rect(txui::Rect(ax + 6.0, cur_y, aw - 12.0, 26.0), 6.0, ROW_SELECT);
            }
            painter.fill_circle(txui::Point(ax + 18.0, cur_y + 13.0), 3.5,
                                all_apps[i].is_terminal ? ACCENT_CYAN : ACCENT_BLUE);
            painter.draw_text(txui::Point(ax + 28.0, cur_y + 6.0), all_apps[i].name,
                              sel ? txui::Color(255, 255, 255, 255) : TXT_PRI, 12.5);
            cur_y += 28.0;
        }
    }

    if (calendar_open && m_calendar_flyout) {
        m_calendar_flyout->paint(painter);
    }

    if (notifications_open && m_notification_flyout) {
        m_notification_flyout->paint(painter);
    }
    if (volume_flyout_open && m_volume_flyout) {
        m_volume_flyout->paint(painter);
    }
    if (brightness_flyout_open && m_brightness_flyout) {
        m_brightness_flyout->paint(painter);
    }
}

bool DesktopShellWidget::handle_event(const txui::Event& event) noexcept {
    const auto& f = frame();
    const double W = f.width();

    // 1. If any flyout is open, give it first opportunity to process input
    if (logo_menu_open && m_logo_menu && m_logo_menu->handle_event(event)) {
        return true;
    }
    if (calendar_open && m_calendar_flyout && m_calendar_flyout->handle_event(event)) {
        return true;
    }
    if (notifications_open && m_notification_flyout && m_notification_flyout->handle_event(event)) {
        return true;
    }
    if (volume_flyout_open && m_volume_flyout && m_volume_flyout->handle_event(event)) {
        return true;
    }
    if (brightness_flyout_open && m_brightness_flyout && m_brightness_flyout->handle_event(event)) {
        return true;
    }

    // 2. Delegate to Center Aura Notch
    if (m_notch && m_notch->handle_event(event)) {
        return true;
    }

    // 3. TopBar Hover & Hit-Testing
    if (event.type == txui::EventType::PointerMove) {
        double mx = event.pointer.x, my = event.pointer.y;

        bool h_logo = (mx >= f.x() + 6.0 && mx <= f.x() + 36.0 && my >= f.y() && my <= f.y() + 32.0);
        double app_label_w = txui::FontMetrics::measure(active_app_name, 13.0).width + 28.0;
        bool h_app  = (mx >= f.x() + 42.0 && mx <= f.x() + 42.0 + app_label_w && my >= f.y() && my <= f.y() + 32.0);
        bool h_sun  = (mx >= f.x() + W - 230.0 && mx <= f.x() + W - 200.0 && my >= f.y() && my <= f.y() + 32.0);
        bool h_vol  = (mx >= f.x() + W - 195.0 && mx <= f.x() + W - 165.0 && my >= f.y() && my <= f.y() + 32.0);
        bool h_bat  = (mx >= f.x() + W - 155.0 && mx <= f.x() + W - 90.0 && my >= f.y() && my <= f.y() + 32.0);
        bool h_wifi = (mx >= f.x() + W - 88.0 && mx <= f.x() + W - 50.0 && my >= f.y() && my <= f.y() + 32.0);
        bool h_bell = (mx >= f.x() + W - 46.0 && mx <= f.x() + W - 10.0 && my >= f.y() && my <= f.y() + 32.0);

        if (hover_logo != h_logo || hover_app_title != h_app || hover_sun != h_sun ||
            hover_vol != h_vol || hover_bat != h_bat || hover_wifi != h_wifi || hover_bell != h_bell) {
            hover_logo = h_logo;
            hover_app_title = h_app;
            hover_sun = h_sun;
            hover_vol = h_vol;
            hover_bat = h_bat;
            hover_wifi = h_wifi;
            hover_bell = h_bell;
            mark_needs_paint();
        }

        if (app_menu_open) {
            int new_h = -1;
            if (mx >= f.x() + 44.0 && mx <= f.x() + 294.0 && my >= f.y() + 38.0) {
                double rel_y = my - (f.y() + 46.0);
                if (rel_y >= 0.0) new_h = static_cast<int>(rel_y / 28.0);
            }
            if (app_menu_hover != new_h) {
                app_menu_hover = new_h;
                mark_needs_paint();
            }
        }
        return true;
    }

    if (event.type == txui::EventType::PointerScroll) {
        if (hover_vol) {
            int step = (event.pointer.scroll_delta_y > 0) ? 5 : -5;
            hardware::AudioUtils::step_volume(step, /*persist=*/true);
            if (m_volume_flyout) m_volume_flyout->refresh_state();
            mark_needs_paint();
            return true;
        }
        if (hover_sun) {
            int step = (event.pointer.scroll_delta_y > 0) ? 5 : -5;
            hardware::BacklightUtils::step_brightness(step, /*persist=*/true);
            if (m_brightness_flyout) m_brightness_flyout->refresh_state();
            mark_needs_paint();
            return true;
        }
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            double mx = event.pointer.x, my = event.pointer.y;

            if (my <= f.y() + 46.0) {
                if (hover_logo) {
                    bool was_open = logo_menu_open;
                    close_all_flyouts();
                    logo_menu_open = !was_open;
                    if (on_resize_requested) on_resize_requested(logo_menu_open ? 300.0 : 46.0);
                    mark_needs_paint();
                    return true;
                }
                if (hover_app_title) {
                    bool was_open = app_menu_open;
                    close_all_flyouts();
                    app_menu_open = !was_open;
                    if (on_resize_requested) on_resize_requested(app_menu_open ? 380.0 : 46.0);
                    mark_needs_paint();
                    return true;
                }
                if (hover_sun) {
                    bool was_open = brightness_flyout_open;
                    close_all_flyouts();
                    brightness_flyout_open = !was_open;
                    if (brightness_flyout_open && m_brightness_flyout) {
                        m_brightness_flyout->refresh_state();
                    }
                    if (on_resize_requested) on_resize_requested(brightness_flyout_open ? 180.0 : 46.0);
                    mark_needs_paint();
                    return true;
                }
                if (hover_vol) {
                    bool was_open = volume_flyout_open;
                    close_all_flyouts();
                    volume_flyout_open = !was_open;
                    if (volume_flyout_open && m_volume_flyout) {
                        m_volume_flyout->refresh_state();
                    }
                    if (on_resize_requested) on_resize_requested(volume_flyout_open ? 190.0 : 46.0);
                    mark_needs_paint();
                    return true;
                }
                if (hover_bat) {
                    log::info("[Shell] Battery clicked: capacity={}%", read_battery_percent());
                    return true;
                }
                if (hover_wifi) {
                    log::info("[Shell] Wi-Fi clicked -> launching Settings Network page");
                    AppItem wifi_app{"Settings", "tinexus-settings-ui", "Network Settings", false, ""};
                    spawn_app(wifi_app);
                    close_all_flyouts();
                    return true;
                }
                if (hover_bell) {
                    bool was_open = notifications_open;
                    close_all_flyouts();
                    notifications_open = !was_open;
                    if (on_resize_requested) {
                        double nh = m_notification_flyout ? m_notification_flyout->calculate_height() + 50.0 : 350.0;
                        on_resize_requested(notifications_open ? nh : 46.0);
                    }
                    mark_needs_paint();
                    return true;
                }
            }

            if (app_menu_open && mx >= f.x() + 44.0 && mx <= f.x() + 294.0 && my >= f.y() + 38.0) {
                if (app_menu_hover >= 0 && app_menu_hover < static_cast<int>(all_apps.size())) {
                    size_t idx = static_cast<size_t>(app_menu_hover);
                    active_app_name = all_apps[idx].name;
                    spawn_app(all_apps[idx]);
                    close_all_flyouts();
                    return true;
                }
            }

            if (my > f.y() + 46.0) {
                close_all_flyouts();
                return true;
            }
        }
    }

    return false;
}

} // namespace tinexus::shell
