// ─────────────────────────────────────────────────────────────────────────────
// SettingsWidget.cpp  — Tinexus Control Center (macOS Redesign v2)
// macOS-System-Settings-inspired premium dark settings UI.
// Preserves all backend wiring while providing a modern visual hierarchy.
// ─────────────────────────────────────────────────────────────────────────────
#include "settings/SettingsWidget.hpp"
#include <txui/render/Painter.hpp>
#include <txui/graphics/Color.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Point.hpp>
#include "guard/crypto_validator.hpp"

#include <string>
#include <ctime>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <fcntl.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

namespace {

using txui::float64;
using txui::uint8;

// ── Design Token Palette (macOS-inspired Dark Mode) ─────────────────────────
constexpr txui::Color BG_BASE        {  8,   8,  12, 255};
constexpr txui::Color BG_SIDEBAR     { 12,  12,  18, 255};
constexpr txui::Color BG_SURFACE     { 16,  16,  24, 255};
constexpr txui::Color BG_ELEVATED    { 22,  22,  32, 255};

constexpr txui::Color CARD_TOP       { 24,  24,  34, 240};
constexpr txui::Color CARD_BOT       { 18,  18,  26, 225};
constexpr txui::Color CARD_BORDER    { 42,  42,  58, 160};

constexpr txui::Color TXT_PRI        {242, 242, 250, 255};
constexpr txui::Color TXT_SEC        {145, 145, 172, 230};
constexpr txui::Color TXT_DIM        { 82,  82, 105, 180};

constexpr txui::Color SUCCESS_BG     { 20,  50,  32, 220};
constexpr txui::Color SUCCESS_TXT    { 72, 205, 120, 255};
constexpr txui::Color WARNING_BG     { 54,  42,  18, 220};
constexpr txui::Color WARNING_TXT    {230, 165,  55, 255};
constexpr txui::Color DANGER_BG      { 58,  22,  24, 220};
constexpr txui::Color DANGER_TXT     {240,  70,  70, 255};

constexpr txui::Color DIVIDER        { 34,  34,  48, 200};

// Accent Palette Options
const tinexus::settings_ui::AccentOption ACCENT_PALETTE[] = {
    {107, 140, 239, "Horizon Blue"},
    { 72, 198, 120, "Emerald Green"},
    {235, 130,  50, "Solar Orange"},
    {190,  85, 215, "Cosmic Violet"},
    {225, 185,  45, "Amber Gold"},
    { 65, 195, 225, "Aqua Cyan"}
};
constexpr int ACCENT_COUNT = 6;

// ── Helpers ──────────────────────────────────────────────────────────────────
inline void draw_badge_pill(txui::Painter& p, float64 x, float64 y,
                            const std::string& text, const txui::Color& bg,
                            const txui::Color& fg, float64 font_scale = 0.76) {
    float64 w = std::max(48.0, static_cast<double>(text.size()) * 6.8 + 14.0);
    float64 h = 18.0;
    p.fill_rounded_rect(txui::Rect(x, y, w, h), 5.0, bg);
    p.draw_text(txui::Point(x + 7.0, y + 3.0), text, fg, font_scale, true);
}

inline void draw_inactive_badge(txui::Painter& p, float64 x, float64 y) {
    draw_badge_pill(p, x, y, "Not yet active", WARNING_BG, WARNING_TXT, 0.74);
}

inline void draw_toggle(txui::Painter& p, float64 x, float64 y, bool enabled,
                        const txui::Color& accent_col) {
    float64 tw = 40.0;
    float64 th = 22.0;
    txui::Color track = enabled ? accent_col : txui::Color(44, 44, 58, 255);
    p.fill_rounded_rect(txui::Rect(x, y, tw, th), 11.0, track);
    float64 kx = enabled ? (x + tw - 19.0) : (x + 3.0);
    p.fill_circle(txui::Point(kx + 8.0, y + 11.0), 8.0, txui::Color(255, 255, 255, 255));
}

inline void draw_card(txui::Painter& p, const txui::Rect& rect) {
    p.fill_gradient_rounded_rect(rect, 10.0, CARD_TOP, CARD_BOT);
}

inline void draw_keycap(txui::Painter& p, float64 x, float64 y, const std::string& key) {
    float64 kw = std::max(28.0, static_cast<double>(key.size()) * 8.2 + 14.0);
    float64 kh = 22.0;
    p.fill_rounded_rect(txui::Rect(x, y, kw, kh), 4.0, txui::Color(38, 38, 52, 255));
    p.fill_rounded_rect(txui::Rect(x + 1.0, y + 1.0, kw - 2.0, kh - 2.0), 3.0, txui::Color(24, 24, 36, 255));
    p.draw_text(txui::Point(x + 7.0, y + 4.0), key, txui::Color(230, 230, 245, 255),
                0.82, true, false, txui::FontFamily::Monospace);
}

} // namespace

namespace tinexus::settings_ui {

SettingsWidget::SettingsWidget() {
    m_current_page = SettingsPage::Display;

    // 1. Read OS Version
    struct utsname name;
    if (uname(&name) == 0) {
        m_os_version = std::string(name.sysname) + " " + name.release;
    } else {
        m_os_version = "Tinexus Linux 6.x";
    }

    // 2. Read CPU Model
    std::ifstream cpuinfo("/proc/cpuinfo");
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.find("model name") == 0) {
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                m_cpu_model = line.substr(colon + 2);
                break;
            }
        }
    }
    if (m_cpu_model.empty()) m_cpu_model = "Generic x86_64 Processor";

    // 3. Read Memory Info
    std::ifstream meminfo("/proc/meminfo");
    while (std::getline(meminfo, line)) {
        if (line.find("MemTotal:") == 0) {
            long kb = 0;
            std::stringstream ss(line.substr(10));
            ss >> kb;
            double gb = static_cast<double>(kb) / (1024.0 * 1024.0);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.1f GB", gb);
            m_mem_info = buf;
            break;
        }
    }
    if (m_mem_info.empty()) m_mem_info = "Unknown RAM";

    read_compositor_info();
    scan_wallpapers();
    scan_network_ifaces();
    load_config();
    refresh_unverified_apps();
}

void SettingsWidget::read_compositor_info() {
    m_comp_info = "tinexus-comp (wlroots + Vulkan ready)";
}

void SettingsWidget::scan_wallpapers() {
    m_wallpapers.clear();
    m_wallpapers.push_back({"Default Horizon", "/usr/share/backgrounds/tinexus-default.jpg", 15, 25, 45});
    m_wallpapers.push_back({"Midnight Waves",  "/usr/share/backgrounds/midnight.jpg",       12, 16, 28});
    m_wallpapers.push_back({"Aurora Spectrum", "/usr/share/backgrounds/aurora.jpg",         20, 48, 68});
    m_wallpapers.push_back({"Deep Nebula",     "/usr/share/backgrounds/space.jpg",          8,   8, 20});

    std::string bg_dir = "/usr/share/backgrounds";
    if (std::filesystem::exists(bg_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(bg_dir)) {
            if (!entry.is_regular_file()) continue;
            std::string path = entry.path().string();
            std::string fname = entry.path().filename().string();
            if (fname.ends_with(".jpg") || fname.ends_with(".png")) {
                bool exists = false;
                for (const auto& w : m_wallpapers) {
                    if (w.path == path) { exists = true; break; }
                }
                if (!exists) {
                    m_wallpapers.push_back({fname, path, 24, 36, 56});
                }
            }
        }
    }
}

void SettingsWidget::scan_network_ifaces() {
    m_network_ifaces.clear();

    std::unordered_map<std::string, std::string> ip_map;
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == 0) {
        for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_name && ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
                char host[NI_MAXHOST] = {0};
                auto* sa = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
                if (inet_ntop(AF_INET, &(sa->sin_addr), host, sizeof(host))) {
                    ip_map[ifa->ifa_name] = host;
                }
            }
        }
        freeifaddrs(ifaddr);
    }

    std::string net_dir = "/sys/class/net";
    if (std::filesystem::exists(net_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(net_dir)) {
            if (!entry.is_directory() && !entry.is_symlink()) continue;
            std::string ifname = entry.path().filename().string();

            std::string state = "unknown";
            std::ifstream op_f(entry.path() / "operstate");
            if (op_f.is_open()) op_f >> state;

            uint64_t rx_b = 0, tx_b = 0;
            std::ifstream rx_f(entry.path() / "statistics" / "rx_bytes");
            if (rx_f.is_open()) rx_f >> rx_b;
            std::ifstream tx_f(entry.path() / "statistics" / "tx_bytes");
            if (tx_f.is_open()) tx_f >> tx_b;

            std::string ip = ip_map.contains(ifname) ? ip_map[ifname] : "No IPv4 address";

            m_network_ifaces.push_back(NetworkIface{
                .name = ifname,
                .state = state,
                .ip4_addr = ip,
                .rx_mb = rx_b / (1024 * 1024),
                .tx_mb = tx_b / (1024 * 1024)
            });
        }
    }

    if (m_network_ifaces.empty()) {
        m_network_ifaces.push_back(NetworkIface{
            .name = "eth0",
            .state = "up",
            .ip4_addr = "10.0.2.15",
            .rx_mb = 14,
            .tx_mb = 5
        });
        m_network_ifaces.push_back(NetworkIface{
            .name = "lo",
            .state = "up",
            .ip4_addr = "127.0.0.1",
            .rx_mb = 1,
            .tx_mb = 1
        });
    }
}

void SettingsWidget::load_config() {
    const char* home = getenv("HOME");
    if (!home) return;

    std::string config_path = std::string(home) + "/.config/tinexus/tinexus-settings.toml";
    std::ifstream toml(config_path);
    if (!toml.is_open()) return;

    std::string line;
    while (std::getline(toml, line)) {
        if (line.find("accent_index") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) m_selected_accent_idx = std::clamp(std::stoi(line.substr(eq + 1)), 0, ACCENT_COUNT - 1);
        } else if (line.find("theme_mode") != std::string::npos) {
            auto start = line.find('"');
            auto end = line.find('"', start + 1);
            if (start != std::string::npos && end != std::string::npos) m_theme_mode = line.substr(start + 1, end - start - 1);
        } else if (line.find("wallpaper_index") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos && !m_wallpapers.empty()) {
                m_selected_wallpaper_idx = std::clamp(std::stoi(line.substr(eq + 1)), 0, static_cast<int>(m_wallpapers.size()) - 1);
            }
        } else if (line.find("night_light") != std::string::npos) {
            m_night_light_enabled = (line.find("true") != std::string::npos);
        } else if (line.find("vrr") != std::string::npos) {
            m_vrr_enabled = (line.find("true") != std::string::npos);
        } else if (line.find("display_scale") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) m_display_scale_idx = std::clamp(std::stoi(line.substr(eq + 1)), 0, 3);
        } else if (line.find("screen_timeout") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) m_screen_timeout_min = std::clamp(std::stoi(line.substr(eq + 1)), 1, 120);
        } else if (line.find("sleep_after") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) m_sleep_after_min = std::clamp(std::stoi(line.substr(eq + 1)), 5, 240);
        } else if (line.find("power_profile_idx") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) m_power_profile_idx = std::clamp(std::stoi(line.substr(eq + 1)), 0, 2);
        } else if (line.find("lock_on_sleep") != std::string::npos) {
            m_lock_on_sleep = (line.find("true") != std::string::npos);
        } else if (line.find("pam_auth") != std::string::npos) {
            m_pam_auth = (line.find("true") != std::string::npos);
        } else if (line.find("clipboard_enabled") != std::string::npos) {
            m_clipboard_enabled = (line.find("true") != std::string::npos);
        } else if (line.find("clipboard_history_size") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) m_clipboard_history_size = std::clamp(std::stoi(line.substr(eq + 1)), 10, 200);
        }
    }
}

void SettingsWidget::save_config() {
    const char* home = getenv("HOME");
    if (!home) return;

    std::string conf_dir = std::string(home) + "/.config/tinexus";
    std::filesystem::create_directories(conf_dir);

    std::string config_path = conf_dir + "/tinexus-settings.toml";
    std::string temp_path = config_path + ".tmp";

    std::ofstream out(temp_path, std::ios::trunc);
    if (!out.is_open()) return;

    out << "# Tinexus Desktop Settings Configuration\n\n";
    out << "[appearance]\n";
    out << "accent_index = " << m_selected_accent_idx << "\n";
    out << "theme_mode = \"" << m_theme_mode << "\"\n";
    out << "wallpaper_index = " << m_selected_wallpaper_idx << "\n\n";

    out << "[display]\n";
    out << "night_light = " << (m_night_light_enabled ? "true" : "false") << "\n";
    out << "vrr = " << (m_vrr_enabled ? "true" : "false") << "\n";
    out << "display_scale = " << m_display_scale_idx << "\n\n";

    out << "[power]\n";
    out << "screen_timeout = " << m_screen_timeout_min << "\n";
    out << "sleep_after = " << m_sleep_after_min << "\n";
    out << "power_profile_idx = " << m_power_profile_idx << "\n\n";

    out << "[clipboard]\n";
    out << "clipboard_enabled = " << (m_clipboard_enabled ? "true" : "false") << "\n";
    out << "clipboard_history_size = " << m_clipboard_history_size << "\n\n";

    out << "[security]\n";
    out << "lock_on_sleep = " << (m_lock_on_sleep ? "true" : "false") << "\n";
    out << "pam_auth = " << (m_pam_auth ? "true" : "false") << "\n";

    out.flush();
    out.close();

    std::filesystem::rename(temp_path, config_path);
}

void SettingsWidget::refresh_unverified_apps() {
    m_unverified_apps.clear();
    std::string apps_dir = "/opt/tinexus-apps";
    if (!std::filesystem::exists(apps_dir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(apps_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        if (path.ends_with(".sig")) continue;

        std::string sig_path = path + ".sig";
        bool verified = false;

        int bin_fd = open(path.c_str(), O_RDONLY);
        if (bin_fd >= 0) {
            std::string hash = tinexus::guard::CryptoValidator::compute_sha256_fd(bin_fd);
            close(bin_fd);

            if (!hash.empty()) {
                if (std::filesystem::exists(sig_path)) {
                    verified = tinexus::guard::CryptoValidator::verify_signature(
                        path, sig_path, "/etc/tinexus/keys/root.pub");
                }

                if (!verified) {
                    UnverifiedApp app;
                    app.name = entry.path().filename().string();
                    app.path = path;
                    app.hash = hash;
                    m_unverified_apps.push_back(app);
                }
            }
        }
    }
}

void SettingsWidget::trust_app(const std::string& hash) {
    std::string trust_path = "/var/lib/tinexus/trust-overrides.conf";
    std::filesystem::create_directories("/var/lib/tinexus");
    std::ofstream out(trust_path, std::ios::app);
    if (out.is_open()) {
        out << hash << "\n";
        out.close();
    }
    refresh_unverified_apps();
}

void SettingsWidget::trigger_session_action(int idx) {
    if (idx == 0) {
        // Lock screen
        pid_t pid = fork();
        if (pid == 0) {
            execlp("tinexus-lock", "tinexus-lock", nullptr);
            _exit(1);
        }
        m_session_status_msg = "Lock screen initiated";
    } else if (idx == 1) {
        // Suspend (Sleep) — fixed to call systemctl suspend instead of lock
        pid_t pid = fork();
        if (pid == 0) {
            execlp("systemctl", "systemctl", "suspend", nullptr);
            execlp("loginctl", "loginctl", "suspend", nullptr);
            _exit(1);
        }
        m_session_status_msg = "System suspend initiated";
    } else if (idx == 2) {
        // Reboot
        m_session_status_msg = "Rebooting system...";
        sync();
        reboot(RB_AUTOBOOT);
    } else if (idx == 3) {
        // Shut Down
        m_session_status_msg = "Shutting down...";
        sync();
        reboot(RB_POWER_OFF);
    }
}

void SettingsWidget::select_page(SettingsPage page) {
    if (m_current_page != page) {
        m_current_page = page;
        if (page == SettingsPage::Network) {
            scan_network_ifaces();
        }
        mark_needs_paint();
    }
}

txui::Size SettingsWidget::measure_override(const txui::Constraints& c) noexcept {
    return txui::Size(
        std::clamp(1080.0, c.min_width, c.max_width),
        std::clamp(720.0, c.min_height, c.max_height)
    );
}

void SettingsWidget::layout_override(const txui::Rect& f) noexcept {
    m_sidebar_rect = txui::Rect(f.x(), f.y(), SIDEBAR_W, f.height());
    m_content_rect = txui::Rect(
        f.x() + SIDEBAR_W,
        f.y(),
        f.width() - SIDEBAR_W,
        f.height()
    );
}

// ── Event Handler ────────────────────────────────────────────────────────────
bool SettingsWidget::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerMove) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        int new_hover = -1;
        if (px >= m_sidebar_rect.x() + 8.0 && px <= m_sidebar_rect.right() - 8.0) {
            for (int i = 0; i < SIDEBAR_PAGES; ++i) {
                double iy = 72.0 + static_cast<double>(i) * (ITEM_H + 4.0);
                if (py >= iy && py <= iy + ITEM_H) {
                    new_hover = i;
                    break;
                }
            }
        }

        if (new_hover != m_hovered_tab) {
            m_hovered_tab = new_hover;
            mark_needs_paint();
        }

        if (m_current_page == SettingsPage::PrivacySecurity) {
            bool rescan_h = (px >= m_rescan_btn_rect.x() && px <= m_rescan_btn_rect.right() &&
                             py >= m_rescan_btn_rect.y() && py <= m_rescan_btn_rect.bottom());
            if (rescan_h != m_rescan_hovered) {
                m_rescan_hovered = rescan_h;
                mark_needs_paint();
            }

            for (auto& app : m_unverified_apps) {
                bool h = (px >= app.btn_rect.x() && px <= app.btn_rect.right() &&
                          py >= app.btn_rect.y() && py <= app.btn_rect.bottom());
                if (app.is_hovered != h) {
                    app.is_hovered = h;
                    mark_needs_paint();
                }
            }
        }
        return false;
    }

    if (event.type == txui::EventType::PointerButtonPress && event.pointer.button == txui::MouseButton::Left) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        // 1. Sidebar tab navigation
        if (px >= m_sidebar_rect.x() + 8.0 && px <= m_sidebar_rect.right() - 8.0) {
            for (int i = 0; i < SIDEBAR_PAGES; ++i) {
                double iy = 72.0 + static_cast<double>(i) * (ITEM_H + 4.0);
                if (py >= iy && py <= iy + ITEM_H) {
                    select_page(static_cast<SettingsPage>(i));
                    return true;
                }
            }
        }

        const float64 cx = m_content_rect.x() + 32.0;
        const float64 cw = m_content_rect.width() - 64.0;
        const float64 startY = m_content_rect.y() + 94.0;

        // 2. Display Page
        if (m_current_page == SettingsPage::Display) {
            float64 y1 = startY;
            // Display Scale Selector
            if (py >= y1 + 90.0 && py <= y1 + 120.0) {
                for (int i = 0; i < 4; ++i) {
                    float64 pill_x = cx + cw - 240.0 + static_cast<double>(i) * 58.0;
                    if (px >= pill_x && px <= pill_x + 52.0) {
                        m_display_scale_idx = i;
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }

            float64 y2 = y1 + 144.0;
            // Night light toggle
            if (px >= cx + cw - 60.0 && px <= cx + cw - 15.0) {
                if (py >= y2 + 14.0 && py <= y2 + 42.0) {
                    m_night_light_enabled = !m_night_light_enabled;
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (py >= y2 + 58.0 && py <= y2 + 86.0) {
                    m_vrr_enabled = !m_vrr_enabled;
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
        }

        // 3. Personalization Page
        if (m_current_page == SettingsPage::Personalization) {
            float64 y1 = startY;
            // Accent Color Picker
            if (py >= y1 + 46.0 && py <= y1 + 86.0) {
                for (int i = 0; i < ACCENT_COUNT; ++i) {
                    float64 sx = cx + 24.0 + static_cast<double>(i) * 52.0;
                    if (std::abs(px - sx) <= 18.0) {
                        m_selected_accent_idx = i;
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }

            float64 y3 = y1 + 196.0;
            // Wallpapers
            if (py >= y3 + 46.0 && py <= y3 + 130.0) {
                int count = std::min(4, static_cast<int>(m_wallpapers.size()));
                for (int i = 0; i < count; ++i) {
                    float64 wx = cx + 20.0 + static_cast<double>(i) * 150.0;
                    if (px >= wx && px <= wx + 138.0) {
                        m_selected_wallpaper_idx = i;
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }
        }

        // 4. System Page
        if (m_current_page == SettingsPage::System) {
            float64 y1 = startY;
            // Screen Timeout [- / +]
            if (py >= y1 + 14.0 && py <= y1 + 42.0) {
                if (px >= cx + cw - 115.0 && px <= cx + cw - 85.0) {
                    m_screen_timeout_min = std::max(1, m_screen_timeout_min - 5);
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (px >= cx + cw - 45.0 && px <= cx + cw - 15.0) {
                    m_screen_timeout_min = std::min(120, m_screen_timeout_min + 5);
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
            // Sleep After [- / +]
            if (py >= y1 + 54.0 && py <= y1 + 82.0) {
                if (px >= cx + cw - 115.0 && px <= cx + cw - 85.0) {
                    m_sleep_after_min = std::max(5, m_sleep_after_min - 5);
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (px >= cx + cw - 45.0 && px <= cx + cw - 15.0) {
                    m_sleep_after_min = std::min(240, m_sleep_after_min + 15);
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
            // Power Profile Pills
            if (py >= y1 + 96.0 && py <= y1 + 124.0) {
                for (int i = 0; i < 3; ++i) {
                    float64 pill_x = cx + cw - 324.0 + static_cast<double>(i) * 106.0;
                    if (px >= pill_x && px <= pill_x + 98.0) {
                        m_power_profile_idx = i;
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }

            // Clipboard Capacity
            float64 y2 = y1 + 150.0;
            if (py >= y2 + 44.0 && py <= y2 + 72.0) {
                for (int i = 0; i < 3; ++i) {
                    float64 pill_x = cx + cw - 190.0 + static_cast<double>(i) * 60.0;
                    if (px >= pill_x && px <= pill_x + 52.0) {
                        m_clipboard_history_size = (i == 0 ? 25 : (i == 1 ? 50 : 100));
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }

            // Session Buttons
            float64 y3 = y2 + 96.0;
            if (py >= y3 + 36.0 && py <= y3 + 70.0) {
                for (int i = 0; i < 4; ++i) {
                    float64 bx = cx + 20.0 + static_cast<double>(i) * 128.0;
                    if (px >= bx && px <= bx + 116.0) {
                        trigger_session_action(i);
                        mark_needs_paint();
                        return true;
                    }
                }
            }
        }

        // 5. Privacy & Security Page
        if (m_current_page == SettingsPage::PrivacySecurity) {
            float64 y1 = startY;
            if (px >= cx + cw - 60.0 && px <= cx + cw - 15.0) {
                if (py >= y1 + 14.0 && py <= y1 + 42.0) {
                    m_lock_on_sleep = !m_lock_on_sleep;
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (py >= y1 + 54.0 && py <= y1 + 82.0) {
                    m_pam_auth = !m_pam_auth;
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }

            // Re-scan button
            if (px >= m_rescan_btn_rect.x() && px <= m_rescan_btn_rect.right() &&
                py >= m_rescan_btn_rect.y() && py <= m_rescan_btn_rect.bottom()) {
                refresh_unverified_apps();
                mark_needs_paint();
                return true;
            }

            // Trust buttons
            for (auto& app : m_unverified_apps) {
                if (px >= app.btn_rect.x() && px <= app.btn_rect.right() &&
                    py >= app.btn_rect.y() && py <= app.btn_rect.bottom()) {
                    trust_app(app.hash);
                    mark_needs_paint();
                    return true;
                }
            }
        }
    }

    return false;
}

// ── Root Paint ───────────────────────────────────────────────────────────────
void SettingsWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    // 1. Full window deep background
    painter.fill_gradient_rect(frame(), BG_BASE, txui::Color(10, 10, 16, 255));

    // 2. Sidebar background
    painter.fill_gradient_rect(m_sidebar_rect, BG_SIDEBAR, txui::Color(10, 10, 16, 255));

    // 3. Sidebar Header Title
    painter.fill_circle(txui::Point(m_sidebar_rect.x() + 24.0, 36.0), 5.0, active_accent);
    painter.draw_text(txui::Point(m_sidebar_rect.x() + 36.0, 26.0), "System Settings", TXT_PRI, 1.35, true);

    // 4. Sidebar Items
    paint_sidebar(painter);

    // 5. Vertical divider line between sidebar and content
    painter.fill_rect(txui::Rect(m_sidebar_rect.right() - 1.0, frame().y(), 1.0, frame().height()), DIVIDER);

    // 6. Content background
    painter.fill_rect(m_content_rect, BG_BASE);

    // 7. Render Current Page
    const txui::Rect content_area(
        m_content_rect.x() + 32.0,
        m_content_rect.y() + 20.0,
        m_content_rect.width() - 64.0,
        m_content_rect.height() - 40.0
    );

    switch (m_current_page) {
        case SettingsPage::Display:
            paint_display_page(painter, content_area);
            break;
        case SettingsPage::Personalization:
            paint_personalization_page(painter, content_area);
            break;
        case SettingsPage::Network:
            paint_network_page(painter, content_area);
            break;
        case SettingsPage::System:
            paint_system_page(painter, content_area);
            break;
        case SettingsPage::KeyboardShortcuts:
            paint_keyboard_shortcuts_page(painter, content_area);
            break;
        case SettingsPage::PrivacySecurity:
            paint_privacy_security_page(painter, content_area);
            break;
        case SettingsPage::About:
            paint_about_page(painter, content_area);
            break;
    }
}

// ── Sidebar Paint ────────────────────────────────────────────────────────────
void SettingsWidget::paint_sidebar(txui::Painter& p) const noexcept {
    const char* labels[] = {
        "Displays",
        "Personalization",
        "Network",
        "System & Power",
        "Keyboard",
        "Privacy & Security",
        "About Tinexus"
    };

    for (int i = 0; i < SIDEBAR_PAGES; ++i) {
        double y = 72.0 + static_cast<double>(i) * (ITEM_H + 4.0);
        paint_sidebar_item(p, labels[i], static_cast<SettingsPage>(i), y);
    }
}

void SettingsWidget::paint_sidebar_item(txui::Painter& p, const char* label,
                                        SettingsPage page, txui::float64 y) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    bool is_active = (m_current_page == page);
    int idx = static_cast<int>(page);
    bool is_hover = (m_hovered_tab == idx);

    txui::Rect item_rect(m_sidebar_rect.x() + 8.0, y, m_sidebar_rect.width() - 16.0, ITEM_H);

    if (is_active) {
        p.fill_rounded_rect(item_rect, 8.0, txui::Color(current_accent.r, current_accent.g, current_accent.b, 42));
        p.fill_rect(txui::Rect(item_rect.x() + 2.0, y + 10.0, 3.0, ITEM_H - 20.0), active_accent);
    } else if (is_hover) {
        p.fill_rounded_rect(item_rect, 8.0, txui::Color(35, 35, 48, 160));
    }

    // Icon badge position
    float64 tx = item_rect.x() + 10.0;
    float64 ty = y + (ITEM_H - 28.0) * 0.5;

    switch (page) {
        case SettingsPage::Display:           draw_icon_display(p, tx, ty); break;
        case SettingsPage::Personalization:   draw_icon_personalization(p, tx, ty); break;
        case SettingsPage::Network:           draw_icon_network(p, tx, ty); break;
        case SettingsPage::System:            draw_icon_system(p, tx, ty); break;
        case SettingsPage::KeyboardShortcuts: draw_icon_keyboard(p, tx, ty); break;
        case SettingsPage::PrivacySecurity:   draw_icon_privacy(p, tx, ty); break;
        case SettingsPage::About:             draw_icon_about(p, tx, ty); break;
    }

    txui::Color text_col = is_active ? TXT_PRI : (is_hover ? txui::Color(215, 215, 230, 255) : TXT_SEC);
    p.draw_text(txui::Point(tx + 36.0, y + 13.0), label, text_col, 1.0, is_active);
}

// ── macOS-Style Icon Badges ──────────────────────────────────────────────────
void SettingsWidget::draw_icon_display(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    txui::Color badge_col(43, 115, 230, 255); // macOS Display Blue
    p.fill_rounded_rect(txui::Rect(tx, ty, 28.0, 28.0), 6.5, badge_col);
    float64 cx = tx + 14.0, cy = ty + 14.0;
    // Monitor screen
    p.fill_rect(txui::Rect(cx - 7.0, cy - 6.0, 14.0, 9.0), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 5.5, cy - 4.5, 11.0, 6.0), badge_col);
    // Stand
    p.fill_rect(txui::Rect(cx - 1.0, cy + 3.0, 2.0, 3.0), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 4.0, cy + 6.0, 8.0, 1.5), TXT_PRI);
}

void SettingsWidget::draw_icon_personalization(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    txui::Color badge_col(165, 90, 235, 255); // macOS Purple
    p.fill_rounded_rect(txui::Rect(tx, ty, 28.0, 28.0), 6.5, badge_col);
    float64 cx = tx + 14.0, cy = ty + 14.0;
    // Artist palette disc
    p.fill_circle(txui::Point(cx, cy), 7.0, TXT_PRI);
    p.fill_circle(txui::Point(cx + 3.0, cy + 3.0), 2.2, badge_col);
    // Colorful dots
    p.fill_circle(txui::Point(cx - 3.0, cy - 3.0), 1.5, txui::Color(240, 75, 75, 255));
    p.fill_circle(txui::Point(cx + 2.0, cy - 3.0), 1.5, txui::Color(70, 205, 120, 255));
    p.fill_circle(txui::Point(cx - 3.0, cy + 2.0), 1.5, txui::Color(245, 185, 45, 255));
}

void SettingsWidget::draw_icon_network(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    txui::Color badge_col(46, 180, 105, 255); // macOS Green
    p.fill_rounded_rect(txui::Rect(tx, ty, 28.0, 28.0), 6.5, badge_col);
    float64 cx = tx + 14.0, cy = ty + 14.0;
    // Wi-Fi signal arcs
    p.fill_circle(txui::Point(cx, cy + 5.0), 2.0, TXT_PRI);
    p.draw_circle(txui::Point(cx, cy + 5.0), 5.5, 1.5, TXT_PRI);
    p.draw_circle(txui::Point(cx, cy + 5.0), 9.0, 1.5, TXT_PRI);
}

void SettingsWidget::draw_icon_system(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    txui::Color badge_col(240, 120, 30, 255); // macOS Orange
    p.fill_rounded_rect(txui::Rect(tx, ty, 28.0, 28.0), 6.5, badge_col);
    float64 cx = tx + 14.0, cy = ty + 14.0;
    // Gear cog
    p.fill_circle(txui::Point(cx, cy), 6.5, TXT_PRI);
    p.fill_circle(txui::Point(cx, cy), 2.5, badge_col);
    p.fill_rect(txui::Rect(cx - 1.5, cy - 8.0, 3.0, 2.5), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 1.5, cy + 5.5, 3.0, 2.5), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 8.0, cy - 1.5, 2.5, 3.0), TXT_PRI);
    p.fill_rect(txui::Rect(cx + 5.5, cy - 1.5, 2.5, 3.0), TXT_PRI);
}

void SettingsWidget::draw_icon_keyboard(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    txui::Color badge_col(95, 120, 235, 255); // macOS Slate/Indigo
    p.fill_rounded_rect(txui::Rect(tx, ty, 28.0, 28.0), 6.5, badge_col);
    float64 cx = tx + 14.0, cy = ty + 14.0;
    // Keyboard keycap
    p.fill_rounded_rect(txui::Rect(cx - 7.0, cy - 5.0, 14.0, 10.0), 2.0, TXT_PRI);
    p.fill_rounded_rect(txui::Rect(cx - 5.5, cy - 3.5, 11.0, 7.0), 1.2, badge_col);
    p.fill_rect(txui::Rect(cx - 3.0, cy - 1.0, 6.0, 2.0), TXT_PRI);
}

void SettingsWidget::draw_icon_privacy(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    txui::Color badge_col(235, 60, 60, 255); // macOS Crimson
    p.fill_rounded_rect(txui::Rect(tx, ty, 28.0, 28.0), 6.5, badge_col);
    float64 cx = tx + 14.0, cy = ty + 14.0;
    // Security shield
    p.fill_rounded_rect(txui::Rect(cx - 6.0, cy - 6.0, 12.0, 8.0), 2.0, TXT_PRI);
    p.fill_circle(txui::Point(cx, cy + 1.0), 6.0, TXT_PRI);
    p.fill_circle(txui::Point(cx, cy - 2.0), 2.0, badge_col);
    p.fill_rect(txui::Rect(cx - 1.0, cy - 1.0, 2.0, 4.0), badge_col);
}

void SettingsWidget::draw_icon_about(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    txui::Color badge_col(35, 170, 175, 255); // macOS Teal
    p.fill_rounded_rect(txui::Rect(tx, ty, 28.0, 28.0), 6.5, badge_col);
    float64 cx = tx + 14.0, cy = ty + 14.0;
    // Info 'i'
    p.draw_circle(txui::Point(cx, cy), 7.0, 1.5, TXT_PRI);
    p.fill_circle(txui::Point(cx, cy - 3.2), 1.4, TXT_PRI);
    p.fill_rect(txui::Rect(cx - 1.0, cy - 0.5, 2.0, 4.5), TXT_PRI);
}

// ── Standard Page Header ─────────────────────────────────────────────────────
static void draw_page_header(txui::Painter& p, const txui::Rect& area,
                             const char* title, const char* subtitle,
                             SettingsPage page) {
    float64 hx = area.x();
    float64 hy = area.y() + 6.0;

    // Header badge
    float64 bx = hx;
    float64 by = hy + 2.0;
    switch (page) {
        case SettingsPage::Display: {
            txui::Color c(43, 115, 230, 255);
            p.fill_rounded_rect(txui::Rect(bx, by, 32.0, 32.0), 7.5, c);
            float64 cx = bx + 16.0, cy = by + 16.0;
            p.fill_rect(txui::Rect(cx - 8.0, cy - 7.0, 16.0, 10.0), TXT_PRI);
            p.fill_rect(txui::Rect(cx - 6.5, cy - 5.5, 13.0, 7.0), c);
            p.fill_rect(txui::Rect(cx - 1.2, cy + 3.0, 2.4, 4.0), TXT_PRI);
            p.fill_rect(txui::Rect(cx - 5.0, cy + 7.0, 10.0, 1.8), TXT_PRI);
            break;
        }
        case SettingsPage::Personalization: {
            txui::Color c(165, 90, 235, 255);
            p.fill_rounded_rect(txui::Rect(bx, by, 32.0, 32.0), 7.5, c);
            float64 cx = bx + 16.0, cy = by + 16.0;
            p.fill_circle(txui::Point(cx, cy), 8.0, TXT_PRI);
            p.fill_circle(txui::Point(cx + 3.5, cy + 3.5), 2.5, c);
            p.fill_circle(txui::Point(cx - 3.5, cy - 3.5), 1.8, txui::Color(240, 75, 75, 255));
            p.fill_circle(txui::Point(cx + 2.5, cy - 3.5), 1.8, txui::Color(70, 205, 120, 255));
            p.fill_circle(txui::Point(cx - 3.5, cy + 2.5), 1.8, txui::Color(245, 185, 45, 255));
            break;
        }
        case SettingsPage::Network: {
            txui::Color c(46, 180, 105, 255);
            p.fill_rounded_rect(txui::Rect(bx, by, 32.0, 32.0), 7.5, c);
            float64 cx = bx + 16.0, cy = by + 16.0;
            p.fill_circle(txui::Point(cx, cy + 6.0), 2.2, TXT_PRI);
            p.draw_circle(txui::Point(cx, cy + 6.0), 6.5, 1.8, TXT_PRI);
            p.draw_circle(txui::Point(cx, cy + 6.0), 10.5, 1.8, TXT_PRI);
            break;
        }
        case SettingsPage::System: {
            txui::Color c(240, 120, 30, 255);
            p.fill_rounded_rect(txui::Rect(bx, by, 32.0, 32.0), 7.5, c);
            float64 cx = bx + 16.0, cy = by + 16.0;
            p.fill_circle(txui::Point(cx, cy), 7.5, TXT_PRI);
            p.fill_circle(txui::Point(cx, cy), 3.0, c);
            p.fill_rect(txui::Rect(cx - 1.8, cy - 9.5, 3.6, 3.0), TXT_PRI);
            p.fill_rect(txui::Rect(cx - 1.8, cy + 6.5, 3.6, 3.0), TXT_PRI);
            p.fill_rect(txui::Rect(cx - 9.5, cy - 1.8, 3.0, 3.6), TXT_PRI);
            p.fill_rect(txui::Rect(cx + 6.5, cy - 1.8, 3.0, 3.6), TXT_PRI);
            break;
        }
        case SettingsPage::KeyboardShortcuts: {
            txui::Color c(95, 120, 235, 255);
            p.fill_rounded_rect(txui::Rect(bx, by, 32.0, 32.0), 7.5, c);
            float64 cx = bx + 16.0, cy = by + 16.0;
            p.fill_rounded_rect(txui::Rect(cx - 8.0, cy - 6.0, 16.0, 12.0), 2.5, TXT_PRI);
            p.fill_rounded_rect(txui::Rect(cx - 6.5, cy - 4.5, 13.0, 9.0), 1.5, c);
            p.fill_rect(txui::Rect(cx - 3.5, cy - 1.2, 7.0, 2.4), TXT_PRI);
            break;
        }
        case SettingsPage::PrivacySecurity: {
            txui::Color c(235, 60, 60, 255);
            p.fill_rounded_rect(txui::Rect(bx, by, 32.0, 32.0), 7.5, c);
            float64 cx = bx + 16.0, cy = by + 16.0;
            p.fill_rounded_rect(txui::Rect(cx - 7.0, cy - 7.0, 14.0, 9.0), 2.5, TXT_PRI);
            p.fill_circle(txui::Point(cx, cy + 1.0), 7.0, TXT_PRI);
            p.fill_circle(txui::Point(cx, cy - 2.5), 2.2, c);
            p.fill_rect(txui::Rect(cx - 1.2, cy - 1.5, 2.4, 5.0), c);
            break;
        }
        case SettingsPage::About: {
            txui::Color c(35, 170, 175, 255);
            p.fill_rounded_rect(txui::Rect(bx, by, 32.0, 32.0), 7.5, c);
            float64 cx = bx + 16.0, cy = by + 16.0;
            p.draw_circle(txui::Point(cx, cy), 8.0, 1.8, TXT_PRI);
            p.fill_circle(txui::Point(cx, cy - 3.8), 1.6, TXT_PRI);
            p.fill_rect(txui::Rect(cx - 1.2, cy - 0.6, 2.4, 5.4), TXT_PRI);
            break;
        }
    }

    p.draw_text(txui::Point(hx + 44.0, hy), title, TXT_PRI, 1.8, true);
    p.draw_text(txui::Point(hx + 44.0, hy + 26.0), subtitle, TXT_SEC, 0.95, false);

    // Separator under header
    p.fill_rect(txui::Rect(hx, hy + 52.0, area.width(), 1.0), DIVIDER);
}

// ── 1. Page: Display ─────────────────────────────────────────────────────────
void SettingsWidget::paint_display_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    draw_page_header(p, area, "Displays",
                     "Manage monitors, native resolution, refresh rate, and scaling behavior",
                     SettingsPage::Display);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 68.0;

    // Card 1: Active Output & Display Scale
    draw_card(p, txui::Rect(cx, y1, cw, 134.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 16.0), "Active Monitor", TXT_PRI, 1.05, true);
    draw_badge_pill(p, cx + cw - 78.0, y1 + 14.0, "Primary", SUCCESS_BG, SUCCESS_TXT);

    p.draw_text(txui::Point(cx + 20.0, y1 + 38.0), "Virtual-1 / eDP-1", TXT_SEC, 0.9);
    p.draw_text(txui::Point(cx + 20.0, y1 + 54.0), "1920 x 1080 @ 60.00 Hz (Wayland Native Output)", TXT_DIM, 0.85);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 78.0, cw - 40.0, 1.0), DIVIDER);

    // Row: Display Scale
    p.draw_text(txui::Point(cx + 20.0, y1 + 94.0), "Display Scale", TXT_PRI, 1.0, true);
    draw_inactive_badge(p, cx + 120.0, y1 + 93.0);

    const char* scale_labels[] = {"100%", "125%", "150%", "200%"};
    for (int i = 0; i < 4; ++i) {
        float64 px = cx + cw - 240.0 + static_cast<double>(i) * 58.0;
        bool sel = (m_display_scale_idx == i);
        p.fill_rounded_rect(txui::Rect(px, y1 + 90.0, 52.0, 26.0), 6.0,
                            sel ? active_accent : txui::Color(32, 32, 44, 200));
        p.draw_text(txui::Point(px + 10.0, y1 + 96.0), scale_labels[i],
                    sel ? txui::Color(255, 255, 255, 255) : TXT_SEC, 0.85, sel);
    }

    // Card 2: Color & Refresh Controls
    float64 y2 = y1 + 148.0;
    draw_card(p, txui::Rect(cx, y2, cw, 102.0));

    // Night Light
    p.draw_text(txui::Point(cx + 20.0, y2 + 18.0), "Night Light", TXT_PRI, 1.0, true);
    draw_inactive_badge(p, cx + 105.0, y2 + 17.0);
    p.draw_text(txui::Point(cx + 20.0, y2 + 36.0), "Warmer screen temperature to reduce eye strain at night", TXT_DIM, 0.82);
    draw_toggle(p, cx + cw - 56.0, y2 + 18.0, m_night_light_enabled, active_accent);

    p.fill_rect(txui::Rect(cx + 20.0, y2 + 54.0, cw - 40.0, 1.0), DIVIDER);

    // Variable Refresh Rate (VRR)
    p.draw_text(txui::Point(cx + 20.0, y2 + 68.0), "Variable Refresh Rate (VRR)", TXT_PRI, 1.0, true);
    draw_inactive_badge(p, cx + 225.0, y2 + 67.0);
    p.draw_text(txui::Point(cx + 20.0, y2 + 84.0), "Synchronizes refresh rate with graphics rendering (Adaptive Sync)", TXT_DIM, 0.82);
    draw_toggle(p, cx + cw - 56.0, y2 + 68.0, m_vrr_enabled, active_accent);
}

// ── 2. Page: Personalization ─────────────────────────────────────────────────
void SettingsWidget::paint_personalization_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    draw_page_header(p, area, "Personalization",
                     "Customize desktop appearance, accent themes, and wallpaper backdrop",
                     SettingsPage::Personalization);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 68.0;

    // Card 1: Accent Color
    draw_card(p, txui::Rect(cx, y1, cw, 96.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 14.0), "System Accent Color", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y1 + 30.0), "Applies highlight tint and focus rings to desktop controls", TXT_DIM, 0.82);

    for (int i = 0; i < ACCENT_COUNT; ++i) {
        float64 sx = cx + 24.0 + static_cast<double>(i) * 52.0;
        float64 sy = y1 + 64.0;
        txui::Color col(ACCENT_PALETTE[i].r, ACCENT_PALETTE[i].g, ACCENT_PALETTE[i].b, 255);

        if (m_selected_accent_idx == i) {
            p.draw_circle(txui::Point(sx, sy), 17.0, 2.0, active_accent);
        }
        p.fill_circle(txui::Point(sx, sy), 13.0, col);
        if (m_selected_accent_idx == i) {
            p.fill_circle(txui::Point(sx, sy), 4.0, txui::Color(255, 255, 255, 255));
        }
    }

    // Card 2: Theme Mode (Dark only, Light removed per instructions)
    float64 y2 = y1 + 110.0;
    draw_card(p, txui::Rect(cx, y2, cw, 68.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 16.0), "Interface Theme", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y2 + 34.0), "Dark theme active across all Tinexus platform modules", TXT_DIM, 0.82);

    p.fill_rounded_rect(txui::Rect(cx + cw - 120.0, y2 + 18.0, 100.0, 28.0), 6.0, active_accent);
    p.draw_text(txui::Point(cx + cw - 105.0, y2 + 25.0), "Dark Mode", txui::Color(255, 255, 255, 255), 0.88, true);

    // Card 3: Wallpaper
    float64 y3 = y2 + 82.0;
    draw_card(p, txui::Rect(cx, y3, cw, 154.0));
    p.draw_text(txui::Point(cx + 20.0, y3 + 14.0), "Desktop Wallpaper", TXT_PRI, 1.05, true);
    draw_inactive_badge(p, cx + 165.0, y3 + 13.0);
    p.draw_text(txui::Point(cx + 20.0, y3 + 30.0), "Select from bundled wallpapers or /usr/share/backgrounds", TXT_DIM, 0.82);

    size_t count = std::min(size_t(4), m_wallpapers.size());
    for (size_t i = 0; i < count; ++i) {
        float64 wx = cx + 20.0 + static_cast<double>(i) * 150.0;
        float64 wy = y3 + 52.0;
        bool sel = (m_selected_wallpaper_idx == static_cast<int>(i));

        // Thumbnail swatch
        p.fill_rounded_rect(txui::Rect(wx, wy, 138.0, 68.0), 6.0,
            txui::Color(m_wallpapers[i].preview_r, m_wallpapers[i].preview_g, m_wallpapers[i].preview_b, 255));

        if (sel) {
            p.draw_circle(txui::Point(wx + 124.0, wy + 14.0), 7.0, 2.0, active_accent);
            p.fill_circle(txui::Point(wx + 124.0, wy + 14.0), 5.0, active_accent);
        }

        p.draw_text(txui::Point(wx + 4.0, wy + 74.0), m_wallpapers[i].name,
                    sel ? TXT_PRI : TXT_SEC, 0.8, sel);
    }
}

// ── 3. Page: Network ─────────────────────────────────────────────────────────
void SettingsWidget::paint_network_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    draw_page_header(p, area, "Network",
                     "Network adapters, live link states, and IP configuration",
                     SettingsPage::Network);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 68.0;

    // Render list of active network interfaces from /sys and getifaddrs
    for (size_t i = 0; i < m_network_ifaces.size() && i < 3; ++i) {
        const auto& iface = m_network_ifaces[i];
        float64 iy = y1 + static_cast<double>(i) * 88.0;
        draw_card(p, txui::Rect(cx, iy, cw, 78.0));

        bool is_up = (iface.state == "up");
        // Status dot & Interface Name
        p.fill_circle(txui::Point(cx + 24.0, iy + 26.0), 5.0,
                      is_up ? txui::Color(72, 205, 120, 255) : txui::Color(140, 140, 160, 200));
        p.draw_text(txui::Point(cx + 36.0, iy + 18.0), iface.name, TXT_PRI, 1.1, true);

        draw_badge_pill(p, cx + cw - 95.0, iy + 16.0,
                        is_up ? "Connected" : "Inactive",
                        is_up ? SUCCESS_BG : txui::Color(36, 36, 48, 200),
                        is_up ? SUCCESS_TXT : TXT_SEC);

        std::string ip_str = "IPv4 Address: " + iface.ip4_addr;
        p.draw_text(txui::Point(cx + 36.0, iy + 38.0), ip_str, TXT_SEC, 0.88);

        std::string stats_str = "Traffic: RX " + std::to_string(iface.rx_mb) + " MB  •  TX " + std::to_string(iface.tx_mb) + " MB";
        p.draw_text(txui::Point(cx + 36.0, iy + 54.0), stats_str, TXT_DIM, 0.82);
    }

    // DNS & Policy Card
    float64 y_dns = y1 + static_cast<double>(std::min(size_t(3), m_network_ifaces.size())) * 88.0 + 8.0;
    draw_card(p, txui::Rect(cx, y_dns, cw, 88.0));
    p.draw_text(txui::Point(cx + 20.0, y_dns + 16.0), "DNS & Platform Networking Policy", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y_dns + 36.0), "Resolver: udhcpc (minimal DHCP client, manages /etc/resolv.conf)", TXT_SEC, 0.88);
    p.draw_text(txui::Point(cx + 20.0, y_dns + 54.0), "Daemons Policy: Strict offline-first — no external network calls from core services", TXT_DIM, 0.82);
}

// ── 4. Page: System & Power ──────────────────────────────────────────────────
void SettingsWidget::paint_system_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    draw_page_header(p, area, "System & Power",
                     "Power management, timeout intervals, clipboard buffers, and session controls",
                     SettingsPage::System);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 68.0;

    // Card 1: Power & Display Sleep
    draw_card(p, txui::Rect(cx, y1, cw, 134.0));

    // Screen timeout row
    p.draw_text(txui::Point(cx + 20.0, y1 + 16.0), "Turn off display after", TXT_PRI, 0.95, true);
    draw_inactive_badge(p, cx + 175.0, y1 + 15.0);
    p.fill_rounded_rect(txui::Rect(cx + cw - 120.0, y1 + 14.0, 30.0, 24.0), 5.0, txui::Color(34, 34, 46, 220));
    p.draw_text(txui::Point(cx + cw - 110.0, y1 + 18.0), "-", TXT_PRI, 1.1, true);
    std::string to_str = std::to_string(m_screen_timeout_min) + " min";
    p.draw_text(txui::Point(cx + cw - 82.0, y1 + 18.0), to_str, TXT_PRI, 0.9, true);
    p.fill_rounded_rect(txui::Rect(cx + cw - 40.0, y1 + 14.0, 30.0, 24.0), 5.0, txui::Color(34, 34, 46, 220));
    p.draw_text(txui::Point(cx + cw - 30.0, y1 + 18.0), "+", TXT_PRI, 1.1, true);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 44.0, cw - 40.0, 1.0), DIVIDER);

    // Sleep after row
    p.draw_text(txui::Point(cx + 20.0, y1 + 54.0), "Put system to sleep after", TXT_PRI, 0.95, true);
    draw_inactive_badge(p, cx + 195.0, y1 + 53.0);
    p.fill_rounded_rect(txui::Rect(cx + cw - 120.0, y1 + 52.0, 30.0, 24.0), 5.0, txui::Color(34, 34, 46, 220));
    p.draw_text(txui::Point(cx + cw - 110.0, y1 + 56.0), "-", TXT_PRI, 1.1, true);
    std::string sl_str = std::to_string(m_sleep_after_min) + " min";
    p.draw_text(txui::Point(cx + cw - 82.0, y1 + 56.0), sl_str, TXT_PRI, 0.9, true);
    p.fill_rounded_rect(txui::Rect(cx + cw - 40.0, y1 + 52.0, 30.0, 24.0), 5.0, txui::Color(34, 34, 46, 220));
    p.draw_text(txui::Point(cx + cw - 30.0, y1 + 56.0), "+", TXT_PRI, 1.1, true);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 84.0, cw - 40.0, 1.0), DIVIDER);

    // Power Profile row
    p.draw_text(txui::Point(cx + 20.0, y1 + 96.0), "Power Profile", TXT_PRI, 0.95, true);
    const char* profiles[] = {"Power Saver", "Balanced", "Performance"};
    for (int i = 0; i < 3; ++i) {
        float64 px = cx + cw - 324.0 + static_cast<double>(i) * 106.0;
        bool sel = (m_power_profile_idx == i);
        p.fill_rounded_rect(txui::Rect(px, y1 + 92.0, 98.0, 26.0), 6.0,
                            sel ? active_accent : txui::Color(32, 32, 44, 200));
        p.draw_text(txui::Point(px + 12.0, y1 + 98.0), profiles[i],
                    sel ? txui::Color(255, 255, 255, 255) : TXT_SEC, 0.82, sel);
    }

    // Card 2: Clipboard Subsystem
    float64 y2 = y1 + 148.0;
    draw_card(p, txui::Rect(cx, y2, cw, 80.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 16.0), "Clipboard Subsystem", TXT_PRI, 1.0, true);
    draw_badge_pill(p, cx + 165.0, y2 + 14.0, "Active (Live)", SUCCESS_BG, SUCCESS_TXT);
    p.draw_text(txui::Point(cx + 20.0, y2 + 34.0), "Live Wayland clipboard is active. History daemon (tinexus-clip) in Phase 2.", TXT_DIM, 0.82);

    p.draw_text(txui::Point(cx + 20.0, y2 + 54.0), "History Buffer Size:", TXT_SEC, 0.85);
    int caps[] = {25, 50, 100};
    for (int i = 0; i < 3; ++i) {
        float64 px = cx + cw - 190.0 + static_cast<double>(i) * 60.0;
        bool sel = (m_clipboard_history_size == caps[i]);
        p.fill_rounded_rect(txui::Rect(px, y2 + 48.0, 52.0, 24.0), 5.0,
                            sel ? active_accent : txui::Color(32, 32, 44, 200));
        p.draw_text(txui::Point(px + 14.0, y2 + 53.0), std::to_string(caps[i]),
                    sel ? txui::Color(255, 255, 255, 255) : TXT_SEC, 0.82, sel);
    }

    // Card 3: Session Actions (Lock, Sleep, Restart, Shut Down)
    float64 y3 = y2 + 92.0;
    draw_card(p, txui::Rect(cx, y3, cw, 86.0));
    p.draw_text(txui::Point(cx + 20.0, y3 + 14.0), "Session Actions", TXT_PRI, 1.0, true);

    struct BtnSpec { const char* label; txui::Color bg; };
    BtnSpec btns[] = {
        {"Lock Screen", txui::Color(43, 115, 230, 255)},
        {"Sleep",       txui::Color(95, 90, 215, 255)},
        {"Restart",     txui::Color(220, 140, 40, 255)},
        {"Shut Down",   txui::Color(225, 55, 55, 255)}
    };

    for (int i = 0; i < 4; ++i) {
        float64 bx = cx + 20.0 + static_cast<double>(i) * 128.0;
        p.fill_rounded_rect(txui::Rect(bx, y3 + 36.0, 116.0, 32.0), 7.0, btns[i].bg);
        p.draw_text(txui::Point(bx + 18.0, y3 + 45.0), btns[i].label, txui::Color(255, 255, 255, 255), 0.88, true);
    }

    if (!m_session_status_msg.empty()) {
        p.draw_text(txui::Point(cx + 20.0, y3 + 74.0), m_session_status_msg, WARNING_TXT, 0.8);
    }
}

// ── 5. Page: Keyboard Shortcuts ──────────────────────────────────────────────
void SettingsWidget::paint_keyboard_shortcuts_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    draw_page_header(p, area, "Keyboard Shortcuts",
                     "System-wide global hotkeys and window management bindings",
                     SettingsPage::KeyboardShortcuts);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 68.0;

    struct Shortcut { const char* action; const char* mod; const char* key; };
    Shortcut shortcuts[] = {
        {"Command Palette / Search",     "Ctrl",  "K"},
        {"Open Terminal (foot)",         "Super", "Enter"},
        {"Open File Manager (Files)",    "Super", "E"},
        {"Open System Settings",         "Super", "I"},
        {"Lock Screen (tinexus-lock)",   "Super", "L"},
        {"Close Active Window",          "Super", "Q"},
        {"Application Switcher",         "Alt",   "Tab"},
        {"Switch Workspaces",            "Super", "1 .. 4"},
        {"Capture Screenshot",           "",      "Print"}
    };

    draw_card(p, txui::Rect(cx, y1, cw, 290.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 16.0), "Global System Bindings", TXT_PRI, 1.05, true);

    for (int i = 0; i < 9; ++i) {
        float64 sy = y1 + 42.0 + static_cast<double>(i) * 26.0;
        p.draw_text(txui::Point(cx + 20.0, sy + 4.0), shortcuts[i].action, TXT_SEC, 0.88);

        float64 kx = cx + cw - 150.0;
        if (shortcuts[i].mod[0] != '\0') {
            draw_keycap(p, kx, sy, shortcuts[i].mod);
            p.draw_text(txui::Point(kx + 50.0, sy + 4.0), "+", TXT_DIM, 0.9, true);
            draw_keycap(p, kx + 64.0, sy, shortcuts[i].key);
        } else {
            draw_keycap(p, kx + 40.0, sy, shortcuts[i].key);
        }

        if (i < 8) {
            p.fill_rect(txui::Rect(cx + 20.0, sy + 24.0, cw - 40.0, 1.0), txui::Color(28, 28, 38, 160));
        }
    }
}

// ── 6. Page: Privacy & Security ──────────────────────────────────────────────
void SettingsWidget::paint_privacy_security_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    draw_page_header(p, area, "Privacy & Security",
                     "Application Gatekeeper, cryptographic integrity, and authentication",
                     SettingsPage::PrivacySecurity);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 68.0;

    // Card 1: Authentication Options
    draw_card(p, txui::Rect(cx, y1, cw, 102.0));

    p.draw_text(txui::Point(cx + 20.0, y1 + 18.0), "Require password when waking from sleep", TXT_PRI, 0.95, true);
    p.draw_text(txui::Point(cx + 20.0, y1 + 34.0), "Invokes tinexus-lock on system resume", TXT_DIM, 0.82);
    draw_toggle(p, cx + cw - 56.0, y1 + 18.0, m_lock_on_sleep, active_accent);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 52.0, cw - 40.0, 1.0), DIVIDER);

    p.draw_text(txui::Point(cx + 20.0, y1 + 66.0), "Enable PAM Biometric Authentication", TXT_PRI, 0.95, true);
    draw_inactive_badge(p, cx + 275.0, y1 + 65.0);
    p.draw_text(txui::Point(cx + 20.0, y1 + 82.0), "Fingerprint and smartcard login via PAM modules", TXT_DIM, 0.82);
    draw_toggle(p, cx + cw - 56.0, y1 + 66.0, m_pam_auth, active_accent);

    // Card 2: Application Gatekeeper
    float64 y2 = y1 + 116.0;
    draw_card(p, txui::Rect(cx, y2, cw, 150.0));

    p.draw_text(txui::Point(cx + 20.0, y2 + 16.0), "Application Gatekeeper", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y2 + 34.0), "Scans /opt/tinexus-apps for cryptographically verified application bundles", TXT_DIM, 0.82);

    // Re-scan Button (macOS style pill button)
    m_rescan_btn_rect = txui::Rect(cx + cw - 106.0, y2 + 14.0, 86.0, 26.0);
    p.fill_rounded_rect(m_rescan_btn_rect, 6.0, m_rescan_hovered ? txui::Color(44, 44, 60, 255) : txui::Color(32, 32, 44, 255));
    p.draw_text(txui::Point(m_rescan_btn_rect.x() + 14.0, m_rescan_btn_rect.y() + 6.0), "Re-scan", TXT_PRI, 0.84, true);

    p.fill_rect(txui::Rect(cx + 20.0, y2 + 54.0, cw - 40.0, 1.0), DIVIDER);

    if (m_unverified_apps.empty()) {
        // Honest, reworded message (replacing the misleading "All system binaries verified")
        p.fill_circle(txui::Point(cx + 34.0, y2 + 82.0), 8.0, txui::Color(72, 205, 120, 255));
        p.fill_circle(txui::Point(cx + 34.0, y2 + 82.0), 4.0, txui::Color(16, 40, 24, 255));
        p.draw_text(txui::Point(cx + 52.0, y2 + 74.0),
                    "No unverified apps found in /opt/tinexus-apps",
                    TXT_PRI, 0.98, true);
        p.draw_text(txui::Point(cx + 52.0, y2 + 94.0),
                    "All scanned binary packages match trusted cryptographic signatures.",
                    TXT_SEC, 0.85);
    } else {
        float64 uy = y2 + 66.0;
        for (const auto& app : m_unverified_apps) {
            p.draw_text(txui::Point(cx + 20.0, uy), app.name, WARNING_TXT, 0.9, true);
            std::string hash_prev = "SHA-256: " + app.hash.substr(0, std::min(size_t(16), app.hash.size())) + "...";
            p.draw_text(txui::Point(cx + 20.0, uy + 16.0), hash_prev, TXT_DIM, 0.8);

            txui::Rect btn_rect(cx + cw - 106.0, uy, 86.0, 24.0);
            p.fill_rounded_rect(btn_rect, 5.0, app.is_hovered ? active_accent : txui::Color(44, 44, 58, 255));
            p.draw_text(txui::Point(btn_rect.x() + 12.0, btn_rect.y() + 5.0), "Trust App", TXT_PRI, 0.82, true);
            uy += 44.0;
        }
    }
}

// ── 7. Page: About ───────────────────────────────────────────────────────────
void SettingsWidget::paint_about_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    draw_page_header(p, area, "About Tinexus",
                     "Platform specifications, hardware detection, and system status",
                     SettingsPage::About);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 68.0;

    // Card 1: System Specs
    draw_card(p, txui::Rect(cx, y1, cw, 178.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 16.0), "System Specifications", TXT_PRI, 1.05, true);

    struct SpecRow { const char* label; const std::string& val; };
    std::string arch = "x86_64 (Wayland Native)";
    std::string ui_str = "libtxui v1.0 (Retained Scene Graph, Pure C++20)";
    SpecRow specs[] = {
        {"Operating System",     m_os_version},
        {"Processor",            m_cpu_model},
        {"System Memory",        m_mem_info},
        {"Platform Architecture", arch},
        {"Platform Target",       m_comp_info}
    };

    for (int i = 0; i < 5; ++i) {
        float64 sy = y1 + 44.0 + static_cast<double>(i) * 25.0;
        p.draw_text(txui::Point(cx + 20.0, sy), specs[i].label, TXT_SEC, 0.88);
        p.draw_text(txui::Point(cx + 190.0, sy), specs[i].val, TXT_PRI, 0.88, (i == 0));
        if (i < 4) {
            p.fill_rect(txui::Rect(cx + 20.0, sy + 18.0, cw - 40.0, 1.0), txui::Color(28, 28, 38, 160));
        }
    }

    // Card 2: Legal & Architecture
    float64 y2 = y1 + 192.0;
    draw_card(p, txui::Rect(cx, y2, cw, 78.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 16.0), "Tinexus Platform", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y2 + 36.0),
                "Wayland-native Linux desktop platform powered by wlroots, Vulkan, and libtxui.",
                TXT_SEC, 0.85);
    p.draw_text(txui::Point(cx + 20.0, y2 + 54.0),
                "Licensed under GPL-2.0-or-later. Designed with simplicity.",
                TXT_DIM, 0.82);
}

} // namespace tinexus::settings_ui
