#include "settings/SettingsWidget.hpp"
#include "settings/WifiManager.hpp"
#include "guard/crypto_validator.hpp"
#include "common/NetUtils.hpp"
#include <txui/render/Painter.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Point.hpp>
#include <txui/math/Size.hpp>
#include <txui/graphics/Color.hpp>
#include <txui/input/Event.hpp>

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <filesystem>

namespace tinexus::settings_ui {

using float64 = txui::float64;

// ── macOS High Sierra / Big Sur Dark Palette ─────────────────────────────────
constexpr txui::Color BG_BASE        { 18,  18,  24, 255};
constexpr txui::Color BG_SIDEBAR     { 26,  26,  36, 255};
constexpr txui::Color CARD_TOP       { 30,  30,  42, 230};
constexpr txui::Color CARD_BOT       { 24,  24,  34, 240};
constexpr txui::Color CARD_BORDER    { 46,  46,  64, 180};

constexpr txui::Color TXT_PRI        {245, 245, 250, 255};
constexpr txui::Color TXT_SEC        {160, 160, 178, 255};
constexpr txui::Color TXT_DIM        {110, 110, 128, 255};

constexpr txui::Color SUCCESS_BG     { 24,  54,  36, 230};
constexpr txui::Color SUCCESS_TXT    { 72, 205, 120, 255};

constexpr txui::Color WARNING_BG     { 58,  42,  16, 230};
constexpr txui::Color WARNING_TXT    {245, 185,  45, 255};

constexpr txui::Color DANGER_BG      { 58,  22,  24, 220};
constexpr txui::Color DANGER_TXT     {240,  70,  70, 255};

constexpr txui::Color DIVIDER        { 34,  34,  48, 200};

// Accent Palette Options
const tinexus::settings_ui::AccentOption ACCENT_PALETTE[] = {
    {  0, 122, 255, "Blue"   },
    {175,  82, 222, "Purple" },
    {255,  45,  85, "Pink"   },
    {255,  59,  48, "Red"    },
    {255, 149,   0, "Orange" },
    { 52, 199,  89, "Green"  },
};
constexpr int ACCENT_COUNT = sizeof(ACCENT_PALETTE) / sizeof(ACCENT_PALETTE[0]);

inline float64 estimate_text_width(const std::string& text, float64 font_scale = 1.0, bool bold = true) {
    float64 size = (font_scale <= 5.0) ? (16.0 * font_scale) : font_scale;
    float64 total_w = 0.0;
    for (char c : text) {
        if (c == '*') {
            total_w += std::round(size * 0.533); // Exact FreeType advance (8px at 15px / 0.95 scale)
        } else if (c == ' ' || c == '.' || c == ':' || c == '-' || c == '(' || c == ')') {
            total_w += size * 0.32;
        } else if (c == 'i' || c == 'l' || c == 'I' || c == 'j' || c == 't' || c == 'r' || c == 'f') {
            total_w += size * 0.36;
        } else if (c == 'm' || c == 'M' || c == 'w' || c == 'W') {
            total_w += size * 0.85;
        } else if (c >= 'A' && c <= 'Z') {
            total_w += size * 0.65;
        } else {
            total_w += size * 0.54;
        }
    }
    if (bold && text.find('*') == std::string::npos) total_w *= 1.10;
    return std::ceil(total_w);
}

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

inline void draw_label_and_inactive_badge(
    txui::Painter& p,
    float64 x, float64 y,
    const std::string& label,
    float64 scale = 1.0, bool bold = true,
    const txui::Color& text_col = TXT_PRI,
    float64 badge_y_offset = -1.0
) {
    p.draw_text(txui::Point(x, y), label, text_col, scale, bold);
    float64 text_w = estimate_text_width(label, scale, bold);
    float64 badge_x = x + text_w + 12.0; // 12px clean gap, never overlaps!
    float64 by = (badge_y_offset >= 0.0) ? (y + badge_y_offset) : (y - 1.0);
    draw_badge_pill(p, badge_x, by, "Not yet active", WARNING_BG, WARNING_TXT, 0.74);
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

// ── macOS-Style Unified Segmented Track ──────────────────────────────────────
inline void draw_segmented_track(
    txui::Painter& p,
    float64 x, float64 y, float64 w, float64 h,
    const char* options[], int count, int selected_idx,
    const txui::Color& active_accent
) {
    // Outer container pill with subtle border
    p.fill_rounded_rect(txui::Rect(x, y, w, h), 7.0, txui::Color(46, 46, 64, 180));
    p.fill_rounded_rect(txui::Rect(x + 1.0, y + 1.0, w - 2.0, h - 2.0), 6.0, txui::Color(22, 22, 32, 255));

    float64 seg_w = (w - 4.0) / static_cast<double>(count);
    for (int i = 0; i < count; ++i) {
        float64 sx = x + 2.0 + static_cast<double>(i) * seg_w;
        bool sel = (selected_idx == i);
        if (sel) {
            p.fill_rounded_rect(txui::Rect(sx, y + 2.0, seg_w, h - 4.0), 5.0, active_accent);
        }
        float64 tw = estimate_text_width(options[i], 0.82, sel);
        float64 tx = sx + std::max(2.0, (seg_w - tw) * 0.5);
        float64 ty = y + (h - 14.0) * 0.5;
        p.draw_text(txui::Point(tx, ty), options[i],
                    sel ? txui::Color(255, 255, 255, 255) : TXT_SEC,
                    0.82, sel);
    }
}

inline void draw_keycap(txui::Painter& p, float64 x, float64 y, const std::string& key) {
    float64 w = std::max(26.0, static_cast<double>(key.size()) * 7.5 + 14.0);
    p.fill_rounded_rect(txui::Rect(x, y, w, 22.0), 5.0, txui::Color(44, 44, 58, 255));
    p.fill_rounded_rect(txui::Rect(x + 1.0, y + 1.0, w - 2.0, 19.0), 4.0, txui::Color(32, 32, 42, 255));
    float64 tw = estimate_text_width(key, 0.82, true);
    p.draw_text(txui::Point(x + (w - tw) * 0.5, y + 4.0), key, TXT_PRI, 0.82, true);
}

// ── macOS Wi-Fi Signal Bars (Proportional Heights 5, 8.5, 12, 16) ───────────
void SettingsWidget::draw_wifi_signal_bars(txui::Painter& p, float64 x, float64 y, int bars, const txui::Color& active_col) const noexcept {
    const float64 bar_w = 3.5;
    const float64 gap = 2.5;
    const float64 heights[4] = {5.0, 8.5, 12.0, 16.0};
    const txui::Color inactive_col = txui::Color(255, 255, 255, 45);

    for (int i = 0; i < 4; ++i) {
        float64 bx = x + static_cast<double>(i) * (bar_w + gap);
        float64 bh = heights[i];
        float64 by = y + (16.0 - bh);
        txui::Color col = (i < bars) ? active_col : inactive_col;
        p.fill_rounded_rect(txui::Rect(bx, by, bar_w, bh), 1.5, col);
    }
}

// ── Lock Icon (Secured Wi-Fi Network Glyph) ──────────────────────────────────
void SettingsWidget::draw_lock_icon(txui::Painter& p, float64 x, float64 y, const txui::Color& col) const noexcept {
    // Arch shackle: loop arched above padlock body
    p.draw_circle(txui::Point(x + 5.0, y + 4.0), 3.0, 1.4, col);
    // Padlock body: rounded rect with keyhole dot
    p.fill_rounded_rect(txui::Rect(x + 1.0, y + 5.0, 8.0, 7.0), 1.8, col);
}

// ── Key to ASCII Converter for Password Input ────────────────────────────────
static char key_to_ascii(txui::Key key, bool shift) {
    if (key >= txui::Key::A && key <= txui::Key::Z) {
        char c = 'a' + static_cast<char>(static_cast<uint16_t>(key) - static_cast<uint16_t>(txui::Key::A));
        return shift ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
    }
    if (key >= txui::Key::N0 && key <= txui::Key::N9) {
        if (shift) {
            const char shift_nums[] = ")!@#$%^&*(";
            return shift_nums[static_cast<uint16_t>(key) - static_cast<uint16_t>(txui::Key::N0)];
        }
        return '0' + static_cast<char>(static_cast<uint16_t>(key) - static_cast<uint16_t>(txui::Key::N0));
    }
    if (key == txui::Key::Space) return ' ';
    if (key == txui::Key::Slash) return shift ? '?' : '/';
    if (key == txui::Key::Period) return shift ? '>' : '.';
    if (key == txui::Key::Minus) return shift ? '_' : '-';
    if (key == txui::Key::Backslash) return shift ? '|' : '\\';
    if (key == txui::Key::Comma) return shift ? '<' : ',';
    if (key == txui::Key::Semicolon) return shift ? ':' : ';';
    if (key == txui::Key::Apostrophe) return shift ? '"' : '\'';
    if (key == txui::Key::Grave) return shift ? '~' : '`';
    if (key == txui::Key::Equal) return shift ? '+' : '=';
    if (key == txui::Key::LeftBracket) return shift ? '{' : '[';
    if (key == txui::Key::RightBracket) return shift ? '}' : ']';
    return 0;
}

// ── Drop Shadow & Gradient Helper for Category Icons ────────────────────────
static void draw_icon_badge_base(txui::Painter& p, float64 x, float64 y,
                                 const txui::Color& top_c, const txui::Color& bot_c,
                                 float64 size = 28.0, float64 radius = 6.5) {
    p.fill_rounded_rect(txui::Rect(x - 0.5, y + 1.5, size + 1.0, size + 1.0), radius + 0.5, txui::Color(0, 0, 0, 75));
    p.fill_gradient_rounded_rect(txui::Rect(x, y, size, size), radius, top_c, bot_c);
}

// ── Constructor ─────────────────────────────────────────────────────────────
SettingsWidget::SettingsWidget() {
    read_compositor_info();
    scan_wallpapers();
    scan_network_ifaces();
    load_config();
}

void SettingsWidget::open_wifi_password_modal(const std::string& ssid) {
    m_wifi_modal_open = true;
    m_wifi_modal_ssid = ssid;
    m_wifi_modal_password.clear();
    m_wifi_modal_show_password = false;
    m_wifi_modal_error.clear();
    m_wifi_modal_cancel_hovered = false;
    m_wifi_modal_connect_hovered = false;
    m_wifi_modal_eye_hovered = false;
    m_wifi_modal_scroll_offset = 0;
    m_wifi_modal_cursor_pos = 0;
    mark_needs_paint();
}

void SettingsWidget::read_compositor_info() {
    struct utsname un;
    if (uname(&un) == 0) {
        m_os_version = std::string(un.sysname) + " " + un.release;
    } else {
        m_os_version = "Tinexus Linux (Kernel unknown)";
    }

    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        uint64_t total_mb = (si.totalram * si.mem_unit) / (1024 * 1024);
        uint64_t free_mb  = (si.freeram  * si.mem_unit) / (1024 * 1024);
        uint64_t used_mb  = (total_mb > free_mb) ? (total_mb - free_mb) : 0;
        double used_gb = static_cast<double>(used_mb) / 1024.0;
        double total_gb = static_cast<double>(total_mb) / 1024.0;
        int pct = (total_mb > 0) ? static_cast<int>((used_mb * 100) / total_mb) : 0;

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << used_gb << " GB / " << total_gb << " GB (" << pct << "% used)";
        m_mem_info = ss.str();
    } else {
        m_mem_info = "Unknown RAM";
    }

    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        while (std::getline(cpuinfo, line)) {
            if (line.starts_with("model name") || line.starts_with("Hardware")) {
                auto colon = line.find(':');
                if (colon != std::string::npos) {
                    m_cpu_model = line.substr(colon + 2);
                    break;
                }
            }
        }
    }
    if (m_cpu_model.empty()) m_cpu_model = "Generic x86_64 Processor";
    m_comp_info = "tinexus-comp (wlroots 0.17 + Vulkan Renderer)";
}

void SettingsWidget::scan_wallpapers() {
    m_wallpapers = {
        {"Deep Space",   "/usr/share/backgrounds/tinexus-space.png",   18,  24,  48},
        {"Horizon Glow", "/usr/share/backgrounds/tinexus-horizon.png", 52,  28,  64},
        {"Nordic Mist",  "/usr/share/backgrounds/tinexus-nordic.png",  32,  48,  54},
        {"Vulkan Peak",  "/usr/share/backgrounds/tinexus-vulkan.png",  64,  32,  28}
    };
}

void SettingsWidget::scan_network_ifaces() {
    m_network_ifaces.clear();
    auto phys = tinexus::net::get_physical_interfaces("/sys/class/net");
    for (const auto& p : phys) {
        NetworkIface iface;
        iface.name = p.name;
        iface.ip4_addr = p.ip4_addr;
        iface.state = p.operstate;
        iface.rx_mb = p.rx_mb;
        iface.tx_mb = p.tx_mb;
        m_network_ifaces.push_back(std::move(iface));
    }
}

void SettingsWidget::load_config() {
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::string config_dir = xdg_config ? xdg_config : (std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + "/.config");
    std::string config_path = config_dir + "/tinexus/settings.toml";

    std::ifstream in(config_path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (line.starts_with("accent_index")) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                m_selected_accent_idx = std::stoi(line.substr(pos + 1));
            }
        } else if (line.starts_with("display_scale")) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                m_display_scale_idx = std::stoi(line.substr(pos + 1));
            }
        } else if (line.starts_with("night_light")) {
            m_night_light_enabled = (line.find("true") != std::string::npos);
        } else if (line.starts_with("vrr")) {
            m_vrr_enabled = (line.find("true") != std::string::npos);
        } else if (line.starts_with("screen_timeout")) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                m_screen_timeout_min = std::stoi(line.substr(pos + 1));
            }
        } else if (line.starts_with("sleep_after")) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                m_sleep_after_min = std::stoi(line.substr(pos + 1));
            }
        } else if (line.starts_with("power_profile_idx")) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                m_power_profile_idx = std::stoi(line.substr(pos + 1));
            }
        } else if (line.starts_with("clipboard_history_size")) {
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                m_clipboard_history_size = std::stoi(line.substr(pos + 1));
            }
        } else if (line.starts_with("lock_on_sleep")) {
            m_lock_on_sleep = (line.find("true") != std::string::npos);
        } else if (line.starts_with("pam_auth")) {
            m_pam_auth = (line.find("true") != std::string::npos);
        }
    }
}

void SettingsWidget::save_config() {
    const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
    std::string base_dir = xdg_config ? xdg_config : (std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + "/.config");
    std::string dir_path = base_dir + "/tinexus";
    std::filesystem::create_directories(dir_path);

    std::string config_path = dir_path + "/settings.toml";
    std::string temp_path = config_path + ".tmp";

    std::ofstream out(temp_path);
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
        m_session_status_msg = "Locking screen...";
        int ret = ::system("tinexus-lock &");
        (void)ret;
    } else if (idx == 1) {
        m_session_status_msg = "System suspend initiated";
    } else if (idx == 2) {
        m_session_status_msg = "Rebooting system...";
        sync();
        reboot(LINUX_REBOOT_CMD_RESTART);
    } else if (idx == 3) {
        m_session_status_msg = "Shutting down...";
        sync();
        reboot(LINUX_REBOOT_CMD_POWER_OFF);
    }
}

void SettingsWidget::select_page(SettingsPage page) {
    if (m_current_page != page) {
        m_current_page = page;
        if (m_current_page == SettingsPage::PrivacySecurity) {
            refresh_unverified_apps();
        } else if (m_current_page == SettingsPage::Network) {
            scan_network_ifaces();
        }
        mark_needs_paint();
    }
}

txui::Size SettingsWidget::measure_override(const txui::Constraints& c) noexcept {
    return txui::Size(
        std::isfinite(c.max_width) ? c.max_width : 1000.0,
        std::isfinite(c.max_height) ? c.max_height : 640.0
    );
}

void SettingsWidget::layout_override(const txui::Rect& f) noexcept {
    m_sidebar_rect = txui::Rect(f.x(), f.y(), SIDEBAR_W, f.height());
    m_content_rect = txui::Rect(
        f.x() + SIDEBAR_W,
        f.y(),
        std::max(100.0, f.width() - SIDEBAR_W),
        f.height()
    );
}

// ── Event Handler ────────────────────────────────────────────────────────────
bool SettingsWidget::handle_event(const txui::Event& event) noexcept {
    // ── 0. Wi-Fi Password Connect Modal Interception ─────────────────────────
    if (m_wifi_modal_open) {
        if (event.type == txui::EventType::KeyDown) {
            if (event.keyboard.key == txui::Key::Escape) {
                m_wifi_modal_open = false;
                m_wifi_modal_password.clear();
                m_wifi_modal_error.clear();
                mark_needs_paint();
                return true;
            }
            if (event.keyboard.key == txui::Key::Enter) {
                if (m_wifi_modal_password.empty()) {
                    m_wifi_modal_error = "Password cannot be empty";
                    mark_needs_paint();
                } else {
                    WifiManager::instance().connect(m_wifi_modal_ssid, m_wifi_modal_password);
                    m_wifi_modal_open = false;
                    m_wifi_modal_password.clear();
                    m_wifi_modal_error.clear();
                    mark_needs_paint();
                }
                return true;
            }
            if (event.keyboard.key == txui::Key::Backspace) {
                if (!m_wifi_modal_password.empty()) {
                    m_wifi_modal_password.pop_back();
                    m_wifi_modal_error.clear();
                    mark_needs_paint();
                }
                return true;
            }
            bool shift = txui::has_modifier(event.keyboard.modifiers, txui::KeyModifier::Shift);
            char c = key_to_ascii(event.keyboard.key, shift);
            if (c != 0) {
                if (m_wifi_modal_password.size() < 64) {
                    m_wifi_modal_password.push_back(c);
                    m_wifi_modal_error.clear();
                    mark_needs_paint();
                }
                return true;
            }
            return true; // Consume any other keys while modal is open
        }

        if (event.type == txui::EventType::PointerMove) {
            double px = event.pointer.x;
            double py = event.pointer.y;
            bool cancel_h = m_wifi_modal_cancel_btn.contains(px, py);
            bool conn_h   = m_wifi_modal_connect_btn.contains(px, py);
            bool eye_h    = m_wifi_modal_eye_btn.contains(px, py);
            if (cancel_h != m_wifi_modal_cancel_hovered ||
                conn_h   != m_wifi_modal_connect_hovered ||
                eye_h    != m_wifi_modal_eye_hovered) {
                m_wifi_modal_cancel_hovered  = cancel_h;
                m_wifi_modal_connect_hovered = conn_h;
                m_wifi_modal_eye_hovered     = eye_h;
                mark_needs_paint();
            }
            return true;
        }

        if (event.type == txui::EventType::PointerButtonPress && event.pointer.button == txui::MouseButton::Left) {
            double px = event.pointer.x;
            double py = event.pointer.y;
            if (m_wifi_modal_cancel_btn.contains(px, py)) {
                m_wifi_modal_open = false;
                m_wifi_modal_password.clear();
                m_wifi_modal_error.clear();
                mark_needs_paint();
                return true;
            }
            if (m_wifi_modal_connect_btn.contains(px, py)) {
                if (m_wifi_modal_password.empty()) {
                    m_wifi_modal_error = "Password cannot be empty";
                    mark_needs_paint();
                } else {
                    WifiManager::instance().connect(m_wifi_modal_ssid, m_wifi_modal_password);
                    m_wifi_modal_open = false;
                    m_wifi_modal_password.clear();
                    m_wifi_modal_error.clear();
                    mark_needs_paint();
                }
                return true;
            }
            if (m_wifi_modal_eye_btn.contains(px, py)) {
                m_wifi_modal_show_password = !m_wifi_modal_show_password;
                mark_needs_paint();
                return true;
            }
            if (m_wifi_modal_input_rect.contains(px, py)) {
                float64 rel_x = px - (m_wifi_modal_input_rect.x() + 12.0);
                if (rel_x <= 0.0) {
                    m_wifi_modal_cursor_pos = m_wifi_modal_scroll_offset;
                } else {
                    std::string visible_pw = m_wifi_modal_show_password ? m_wifi_modal_password : std::string(m_wifi_modal_password.size(), '*');
                    if (m_wifi_modal_scroll_offset < visible_pw.size()) {
                        visible_pw = visible_pw.substr(m_wifi_modal_scroll_offset);
                    }
                    size_t idx = 0;
                    while (idx < visible_pw.size() && estimate_text_width(visible_pw.substr(0, idx + 1), 0.95, true) < rel_x) {
                        idx++;
                    }
                    m_wifi_modal_cursor_pos = m_wifi_modal_scroll_offset + idx;
                    if (m_wifi_modal_cursor_pos > m_wifi_modal_password.size()) {
                        m_wifi_modal_cursor_pos = m_wifi_modal_password.size();
                    }
                }
                mark_needs_paint();
                return true;
            }
            // Clicking outside modal dialog dismisses it
            if (!m_wifi_modal_rect.contains(px, py)) {
                m_wifi_modal_open = false;
                m_wifi_modal_password.clear();
                m_wifi_modal_error.clear();
                mark_needs_paint();
                return true;
            }
            return true; // Click inside modal area but not on buttons
        }
        return true;
    }

    if (event.type == txui::EventType::KeyDown) {
        if (event.keyboard.key == txui::Key::N1) {
            select_page(SettingsPage::Display);
            return true;
        } else if (event.keyboard.key == txui::Key::N2) {
            select_page(SettingsPage::Personalization);
            return true;
        } else if (event.keyboard.key == txui::Key::N3) {
            select_page(SettingsPage::Network);
            return true;
        } else if (event.keyboard.key == txui::Key::N4) {
            select_page(SettingsPage::System);
            return true;
        } else if (event.keyboard.key == txui::Key::N5) {
            select_page(SettingsPage::KeyboardShortcuts);
            return true;
        } else if (event.keyboard.key == txui::Key::N6) {
            select_page(SettingsPage::PrivacySecurity);
            return true;
        } else if (event.keyboard.key == txui::Key::N7) {
            select_page(SettingsPage::About);
            return true;
        } else if (event.keyboard.key == txui::Key::Down) {
            int cur = static_cast<int>(m_current_page);
            if (cur < SIDEBAR_PAGES - 1) {
                select_page(static_cast<SettingsPage>(cur + 1));
            }
            return true;
        } else if (event.keyboard.key == txui::Key::Up) {
            int cur = static_cast<int>(m_current_page);
            if (cur > 0) {
                select_page(static_cast<SettingsPage>(cur - 1));
            }
            return true;
        }
    }

    if (event.type == txui::EventType::PointerMove) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        int new_hover = -1;
        if (px >= m_sidebar_rect.x() + 8.0 && px <= m_sidebar_rect.right() - 8.0) {
            for (int i = 0; i < SIDEBAR_PAGES; ++i) {
                double iy = m_sidebar_rect.y() + 52.0 + static_cast<double>(i) * (ITEM_H + 4.0);
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

        if (m_current_page == SettingsPage::Network) {
            bool scan_h = m_wifi_scan_btn_rect.contains(px, py);
            bool disc_h = m_wifi_disconnect_btn_rect.contains(px, py);
            int net_h = -1;
            for (size_t i = 0; i < m_network_item_rects.size(); ++i) {
                if (m_network_item_rects[i].contains(px, py)) {
                    net_h = static_cast<int>(i);
                    break;
                }
            }
            if (scan_h != m_wifi_scan_hovered || disc_h != m_wifi_disconnect_hovered || net_h != m_hovered_network_idx) {
                m_wifi_scan_hovered       = scan_h;
                m_wifi_disconnect_hovered = disc_h;
                m_hovered_network_idx     = net_h;
                mark_needs_paint();
            }
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
                double iy = m_sidebar_rect.y() + 52.0 + static_cast<double>(i) * (ITEM_H + 4.0);
                if (py >= iy && py <= iy + ITEM_H) {
                    select_page(static_cast<SettingsPage>(i));
                    return true;
                }
            }
        }

        const float64 cx = m_content_rect.x() + 32.0;
        const float64 cw = m_content_rect.width() - 64.0;
        const float64 startY = m_content_rect.y() + 114.0;

        // 2. Display Page
        if (m_current_page == SettingsPage::Display) {
            float64 y1 = startY;
            // Display Scale Segmented Track
            float64 track_x = cx + cw - 256.0;
            float64 track_w = 236.0;
            if (px >= track_x && px <= track_x + track_w && py >= y1 + 116.0 && py <= y1 + 144.0) {
                int idx = std::clamp(static_cast<int>((px - track_x) / 58.0), 0, 3);
                m_display_scale_idx = idx;
                save_config();
                mark_needs_paint();
                return true;
            }

            float64 y2 = y1 + 184.0;
            // Night light toggle
            if (px >= cx + cw - 60.0 && px <= cx + cw - 15.0) {
                if (py >= y2 + 18.0 && py <= y2 + 44.0) {
                    m_night_light_enabled = !m_night_light_enabled;
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (py >= y2 + 96.0 && py <= y2 + 122.0) {
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
            if (py >= y1 + 74.0 && py <= y1 + 110.0) {
                for (int i = 0; i < ACCENT_COUNT; ++i) {
                    float64 sx = cx + 24.0 + static_cast<double>(i) * 54.0;
                    if (std::abs(px - sx) <= 18.0) {
                        m_selected_accent_idx = i;
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }

            float64 y2 = y1 + 152.0;
            float64 y3 = y2 + 102.0;
            // Wallpapers
            if (py >= y3 + 76.0 && py <= y3 + 158.0) {
                int count = std::min(4, static_cast<int>(m_wallpapers.size()));
                float64 gap = 16.0;
                float64 total_gaps = gap * static_cast<double>(count > 1 ? count - 1 : 0);
                float64 card_w = (count > 0) ? std::clamp((cw - 40.0 - total_gaps) / static_cast<double>(count), 120.0, 260.0) : 150.0;
                for (int i = 0; i < count; ++i) {
                    float64 wx = cx + 20.0 + static_cast<double>(i) * (card_w + gap);
                    if (px >= wx && px <= wx + card_w) {
                        m_selected_wallpaper_idx = i;
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }
        }

        // 3. Network Page (Wi-Fi interactive controls)
        if (m_current_page == SettingsPage::Network) {
            // Wi-Fi Master Toggle
            if (m_wifi_toggle_rect.contains(px, py)) {
                bool enabled = WifiManager::instance().is_wifi_enabled();
                WifiManager::instance().set_wifi_enabled(!enabled);
                mark_needs_paint();
                return true;
            }

            // Wi-Fi Scan Button
            if (m_wifi_scan_btn_rect.contains(px, py)) {
                WifiManager::instance().trigger_scan();
                mark_needs_paint();
                return true;
            }

            // Disconnect Button
            if (m_wifi_disconnect_btn_rect.contains(px, py)) {
                WifiManager::instance().disconnect();
                mark_needs_paint();
                return true;
            }

            // Click on network item in the list
            for (size_t i = 0; i < m_network_item_rects.size(); ++i) {
                if (m_network_item_rects[i].contains(px, py)) {
                    auto networks = WifiManager::instance().get_networks();
                    if (i < networks.size()) {
                        const auto& net = networks[i];
                        if (net.is_connected) {
                            return true; // Already connected
                        }
                        if (!net.is_secured) {
                            // Open network: connect directly without password prompt
                            WifiManager::instance().connect(net.ssid, "");
                            mark_needs_paint();
                            return true;
                        } else {
                            // Secured network: summon macOS-style password modal
                            m_wifi_modal_open = true;
                            m_wifi_modal_ssid = net.ssid;
                            m_wifi_modal_password.clear();
                            m_wifi_modal_show_password = false;
                            m_wifi_modal_error.clear();
                            m_wifi_modal_cancel_hovered = false;
                            m_wifi_modal_connect_hovered = false;
                            m_wifi_modal_eye_hovered = false;
                            mark_needs_paint();
                            return true;
                        }
                    }
                }
            }
        }

        // 4. System Page
        if (m_current_page == SettingsPage::System) {
            float64 y1 = startY;
            // Screen Timeout Stepper
            if (py >= y1 + 14.0 && py <= y1 + 42.0) {
                if (px >= cx + cw - 130.0 && px <= cx + cw - 100.0) {
                    m_screen_timeout_min = std::max(1, m_screen_timeout_min - 1);
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (px >= cx + cw - 42.0 && px <= cx + cw - 12.0) {
                    m_screen_timeout_min = std::min(60, m_screen_timeout_min + 1);
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
            // Sleep After Stepper
            if (py >= y1 + 70.0 && py <= y1 + 98.0) {
                if (px >= cx + cw - 130.0 && px <= cx + cw - 100.0) {
                    m_sleep_after_min = std::max(1, m_sleep_after_min - 5);
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (px >= cx + cw - 42.0 && px <= cx + cw - 12.0) {
                    m_sleep_after_min = std::min(120, m_sleep_after_min + 5);
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
            // Power Profile Track (macOS Segmented)
            float64 track_x = cx + cw - 324.0;
            float64 track_w = 304.0;
            if (px >= track_x && px <= track_x + track_w && py >= y1 + 124.0 && py <= y1 + 152.0) {
                int idx = std::clamp(static_cast<int>((px - track_x) / 100.0), 0, 2);
                m_power_profile_idx = idx;
                save_config();
                mark_needs_paint();
                return true;
            }

            // Clipboard Capacity
            float64 y2 = y1 + 192.0;
            float64 clip_track_x = cx + cw - 228.0;
            float64 clip_track_w = 208.0;
            if (px >= clip_track_x && px <= clip_track_x + clip_track_w && py >= y2 + 92.0 && py <= y2 + 120.0) {
                int idx = std::clamp(static_cast<int>((px - clip_track_x) / 68.0), 0, 2);
                m_clipboard_history_size = (idx == 0 ? 25 : (idx == 1 ? 50 : 100));
                save_config();
                mark_needs_paint();
                return true;
            }

            // Session Buttons
            float64 y3 = y2 + 156.0;
            if (py >= y3 + 46.0 && py <= y3 + 78.0) {
                float64 gap = 14.0;
                float64 btn_w = std::clamp((cw - 40.0 - 3.0 * gap) / 4.0, 110.0, 220.0);
                for (int i = 0; i < 4; ++i) {
                    float64 bx = cx + 20.0 + static_cast<double>(i) * (btn_w + gap);
                    if (px >= bx && px <= bx + btn_w) {
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
                if (py >= y1 + 18.0 && py <= y1 + 44.0) {
                    m_lock_on_sleep = !m_lock_on_sleep;
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (py >= y1 + 96.0 && py <= y1 + 122.0) {
                    m_pam_auth = !m_pam_auth;
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }

            // Re-scan button
            float64 y2 = y1 + 178.0;
            if (px >= m_rescan_btn_rect.x() && px <= m_rescan_btn_rect.right() &&
                py >= m_rescan_btn_rect.y() && py <= m_rescan_btn_rect.bottom()) {
                refresh_unverified_apps();
                mark_needs_paint();
                return true;
            }

            // Trust buttons
            float64 uy = y2 + 84.0;
            for (auto& app : m_unverified_apps) {
                txui::Rect btn_rect(cx + cw - 106.0, uy, 86.0, 24.0);
                if (px >= btn_rect.x() && px <= btn_rect.right() &&
                    py >= btn_rect.y() && py <= btn_rect.bottom()) {
                    trust_app(app.hash);
                    mark_needs_paint();
                    return true;
                }
                uy += 44.0;
            }
        }
    }

    return false;
}

// ── Paint Override ───────────────────────────────────────────────────────────
void SettingsWidget::paint_override(txui::Painter& painter) const noexcept {
    // 1. Outer boundary background
    painter.fill_rect(frame(), BG_BASE);

    // 2. Sidebar background
    painter.fill_gradient_rect(m_sidebar_rect, BG_SIDEBAR, txui::Color(10, 10, 16, 255));

    // 3. Sidebar Search Pill & Navigation Items
    paint_sidebar(painter);

    // 4. Vertical divider line between sidebar and content
    painter.fill_rect(txui::Rect(m_sidebar_rect.right() - 1.0, frame().y(), 1.0, frame().height()), DIVIDER);

    // 5. Content background
    painter.fill_rect(m_content_rect, BG_BASE);

    // 6. Render Current Page
    const txui::Rect content_area(
        m_content_rect.x() + 32.0,
        m_content_rect.y() + 20.0,
        m_content_rect.width() - 64.0,
        m_content_rect.height() - 32.0
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

    // Modal overlay on top of active page
    if (m_wifi_modal_open) {
        paint_wifi_modal(painter);
    }
}

// ── Sidebar Paint ────────────────────────────────────────────────────────────
void SettingsWidget::paint_sidebar(txui::Painter& p) const noexcept {
    // 1. macOS-style Pill Search Field at the top of the sidebar
    float64 sx = m_sidebar_rect.x() + 12.0;
    float64 sy = m_sidebar_rect.y() + 12.0;
    float64 sw = m_sidebar_rect.width() - 24.0;
    txui::Rect search_rect(sx, sy, sw, 30.0);
    // Outer border ring
    p.fill_rounded_rect(search_rect, 7.5, txui::Color(44, 44, 62, 180));
    // Inner field background
    p.fill_rounded_rect(txui::Rect(sx + 1.0, sy + 1.0, sw - 2.0, 28.0), 6.5, txui::Color(22, 22, 32, 255));

    // Magnifying glass icon
    p.draw_circle(txui::Point(sx + 14.0, sy + 15.0), 3.8, 1.4, TXT_DIM);
    p.fill_rect(txui::Rect(sx + 17.5, sy + 18.0, 3.2, 2.2), TXT_DIM);
    p.draw_text(txui::Point(sx + 27.0, sy + 7.5), "Search settings...", TXT_DIM, 0.85);

    // 2. Navigation Items
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
        double y = m_sidebar_rect.y() + 52.0 + static_cast<double>(i) * (ITEM_H + 4.0);
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

    txui::Color text_col = is_active ? TXT_PRI : (is_hover ? txui::Color(225, 225, 240, 255) : TXT_SEC);
    p.draw_text(txui::Point(tx + 36.0, y + 13.0), label, text_col, 0.96, is_active);
}

// ── macOS-Style Glossy Dimensional Icon Badges ──────────────────────────────
void SettingsWidget::draw_icon_display(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(55, 130, 245, 255), txui::Color(28, 92, 210, 255));
    float64 cx = tx + 14.0, cy = ty + 14.0;
    p.fill_rect(txui::Rect(cx - 7.0, cy - 6.0, 14.0, 9.0), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 5.5, cy - 4.5, 11.0, 6.0), txui::Color(35, 100, 220, 255));
    p.fill_rect(txui::Rect(cx - 1.0, cy + 3.0, 2.0, 3.0), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 4.0, cy + 6.0, 8.0, 1.5), TXT_PRI);
}

void SettingsWidget::draw_icon_personalization(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(180, 105, 250, 255), txui::Color(140, 65, 215, 255));
    float64 cx = tx + 14.0, cy = ty + 14.0;
    p.fill_circle(txui::Point(cx, cy), 7.0, TXT_PRI);
    p.fill_circle(txui::Point(cx + 3.0, cy + 3.0), 2.2, txui::Color(150, 75, 225, 255));
    p.fill_circle(txui::Point(cx - 3.0, cy - 3.0), 1.5, txui::Color(240, 75, 75, 255));
    p.fill_circle(txui::Point(cx + 2.0, cy - 3.0), 1.5, txui::Color(70, 205, 120, 255));
    p.fill_circle(txui::Point(cx - 3.0, cy + 2.0), 1.5, txui::Color(245, 185, 45, 255));
}

void SettingsWidget::draw_icon_network(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(55, 195, 115, 255), txui::Color(32, 155, 88, 255));
    float64 cx = tx + 14.0, cy = ty + 14.0;
    p.fill_circle(txui::Point(cx, cy + 5.0), 2.0, TXT_PRI);
    p.draw_circle(txui::Point(cx, cy + 5.0), 5.5, 1.5, TXT_PRI);
    p.draw_circle(txui::Point(cx, cy + 5.0), 9.0, 1.5, TXT_PRI);
}

void SettingsWidget::draw_icon_system(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(255, 135, 45, 255), txui::Color(215, 95, 20, 255));
    float64 cx = tx + 14.0, cy = ty + 14.0;
    p.fill_circle(txui::Point(cx, cy), 6.5, TXT_PRI);
    p.fill_circle(txui::Point(cx, cy), 2.5, txui::Color(230, 105, 30, 255));
    p.fill_rect(txui::Rect(cx - 1.5, cy - 8.0, 3.0, 2.5), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 1.5, cy + 5.5, 3.0, 2.5), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 8.0, cy - 1.5, 2.5, 3.0), TXT_PRI);
    p.fill_rect(txui::Rect(cx + 5.5, cy - 1.5, 2.5, 3.0), TXT_PRI);
}

void SettingsWidget::draw_icon_keyboard(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(110, 135, 250, 255), txui::Color(75, 100, 215, 255));
    float64 cx = tx + 14.0, cy = ty + 14.0;
    p.fill_rounded_rect(txui::Rect(cx - 7.0, cy - 5.0, 14.0, 10.0), 2.0, TXT_PRI);
    p.fill_rounded_rect(txui::Rect(cx - 5.5, cy - 3.5, 11.0, 7.0), 1.2, txui::Color(85, 110, 230, 255));
    p.fill_rect(txui::Rect(cx - 3.0, cy - 1.0, 6.0, 2.0), TXT_PRI);
}

void SettingsWidget::draw_icon_privacy(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(250, 75, 75, 255), txui::Color(205, 42, 42, 255));
    float64 cx = tx + 14.0, cy = ty + 14.0;
    p.fill_rounded_rect(txui::Rect(cx - 6.0, cy - 6.0, 12.0, 8.0), 2.0, TXT_PRI);
    p.fill_circle(txui::Point(cx, cy + 1.0), 6.0, TXT_PRI);
    p.fill_circle(txui::Point(cx, cy - 2.0), 2.0, txui::Color(220, 50, 50, 255));
    p.fill_rect(txui::Rect(cx - 1.0, cy - 1.0, 2.0, 4.0), txui::Color(220, 50, 50, 255));
}

void SettingsWidget::draw_icon_about(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(45, 185, 190, 255), txui::Color(22, 145, 150, 255));
    float64 cx = tx + 14.0, cy = ty + 14.0;
    p.draw_circle(txui::Point(cx, cy), 7.0, 1.5, TXT_PRI);
    p.fill_circle(txui::Point(cx, cy - 3.2), 1.4, TXT_PRI);
    p.fill_rect(txui::Rect(cx - 1.0, cy - 0.5, 2.0, 4.5), TXT_PRI);
}

// ── Standard Page Header (With generous, spacious title-to-subtitle gap) ────
static void draw_page_header(txui::Painter& p, const txui::Rect& area,
                             const char* title, const char* subtitle,
                             SettingsPage page) {
    float64 hx = area.x();
    float64 hy = area.y() + 4.0;

    float64 bx = hx;
    float64 by = hy + 2.0;

    txui::Color top_c, bot_c;
    switch (page) {
        case SettingsPage::Display:           top_c = txui::Color(55, 130, 245, 255); bot_c = txui::Color(28, 92, 210, 255); break;
        case SettingsPage::Personalization:   top_c = txui::Color(180, 105, 250, 255); bot_c = txui::Color(140, 65, 215, 255); break;
        case SettingsPage::Network:           top_c = txui::Color(55, 195, 115, 255); bot_c = txui::Color(32, 155, 88, 255); break;
        case SettingsPage::System:            top_c = txui::Color(255, 135, 45, 255); bot_c = txui::Color(215, 95, 20, 255); break;
        case SettingsPage::KeyboardShortcuts: top_c = txui::Color(110, 135, 250, 255); bot_c = txui::Color(75, 100, 215, 255); break;
        case SettingsPage::PrivacySecurity:   top_c = txui::Color(250, 75, 75, 255); bot_c = txui::Color(205, 42, 42, 255); break;
        case SettingsPage::About:             top_c = txui::Color(45, 185, 190, 255); bot_c = txui::Color(22, 145, 150, 255); break;
    }

    draw_icon_badge_base(p, bx, by, top_c, bot_c, 32.0, 7.5);

    float64 cx = bx + 16.0, cy = by + 16.0;
    switch (page) {
        case SettingsPage::Display: {
            p.fill_rect(txui::Rect(cx - 8.0, cy - 7.0, 16.0, 10.0), TXT_PRI);
            p.fill_rect(txui::Rect(cx - 6.5, cy - 5.5, 13.0, 7.0), bot_c);
            p.fill_rect(txui::Rect(cx - 1.2, cy + 3.0, 2.4, 4.0), TXT_PRI);
            p.fill_rect(txui::Rect(cx - 5.0, cy + 7.0, 10.0, 1.8), TXT_PRI);
            break;
        }
        case SettingsPage::Personalization: {
            p.fill_circle(txui::Point(cx, cy), 8.0, TXT_PRI);
            p.fill_circle(txui::Point(cx + 3.5, cy + 3.5), 2.5, bot_c);
            p.fill_circle(txui::Point(cx - 3.5, cy - 3.5), 1.8, txui::Color(240, 75, 75, 255));
            p.fill_circle(txui::Point(cx + 2.5, cy - 3.5), 1.8, txui::Color(70, 205, 120, 255));
            p.fill_circle(txui::Point(cx - 3.5, cy + 2.5), 1.8, txui::Color(245, 185, 45, 255));
            break;
        }
        case SettingsPage::Network: {
            p.fill_circle(txui::Point(cx, cy + 6.0), 2.2, TXT_PRI);
            p.draw_circle(txui::Point(cx, cy + 6.0), 6.5, 1.8, TXT_PRI);
            p.draw_circle(txui::Point(cx, cy + 6.0), 10.5, 1.8, TXT_PRI);
            break;
        }
        case SettingsPage::System: {
            p.fill_circle(txui::Point(cx, cy), 7.5, TXT_PRI);
            p.fill_circle(txui::Point(cx, cy), 3.0, bot_c);
            p.fill_rect(txui::Rect(cx - 1.8, cy - 9.5, 3.6, 3.0), TXT_PRI);
            p.fill_rect(txui::Rect(cx - 1.8, cy + 6.5, 3.6, 3.0), TXT_PRI);
            p.fill_rect(txui::Rect(cx - 9.5, cy - 1.8, 3.0, 3.6), TXT_PRI);
            p.fill_rect(txui::Rect(cx + 6.5, cy - 1.8, 3.0, 3.6), TXT_PRI);
            break;
        }
        case SettingsPage::KeyboardShortcuts: {
            p.fill_rounded_rect(txui::Rect(cx - 8.0, cy - 6.0, 16.0, 12.0), 2.5, TXT_PRI);
            p.fill_rounded_rect(txui::Rect(cx - 6.5, cy - 4.5, 13.0, 9.0), 1.5, bot_c);
            p.fill_rect(txui::Rect(cx - 3.5, cy - 1.2, 7.0, 2.4), TXT_PRI);
            break;
        }
        case SettingsPage::PrivacySecurity: {
            p.fill_rounded_rect(txui::Rect(cx - 7.0, cy - 7.0, 14.0, 9.0), 2.5, TXT_PRI);
            p.fill_circle(txui::Point(cx, cy + 1.0), 7.0, TXT_PRI);
            p.fill_circle(txui::Point(cx, cy - 2.5), 2.2, bot_c);
            p.fill_rect(txui::Rect(cx - 1.2, cy - 1.5, 2.4, 5.0), bot_c);
            break;
        }
        case SettingsPage::About: {
            p.draw_circle(txui::Point(cx, cy), 8.0, 1.8, TXT_PRI);
            p.fill_circle(txui::Point(cx, cy - 3.8), 1.6, TXT_PRI);
            p.fill_rect(txui::Rect(cx - 1.2, cy - 0.6, 2.4, 5.4), TXT_PRI);
            break;
        }
    }

    // Title at hy + 2.0, Subtitle at hy + 38.0 (guaranteed 14px clear gap between them!)
    p.draw_text(txui::Point(hx + 46.0, hy + 2.0), title, TXT_PRI, 1.7, true);
    p.draw_text(txui::Point(hx + 46.0, hy + 38.0), subtitle, TXT_SEC, 0.88, false);

    // Separator line at hy + 70.0 (18px clear margin below subtitle)
    p.fill_rect(txui::Rect(hx, hy + 70.0, area.width(), 1.0), DIVIDER);
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
    float64 y1 = area.y() + 94.0;

    // Card 1: Active Output & Display Scale
    draw_card(p, txui::Rect(cx, y1, cw, 164.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "Active Monitor", TXT_PRI, 1.05, true);
    draw_badge_pill(p, cx + cw - 78.0, y1 + 18.0, "Primary", SUCCESS_BG, SUCCESS_TXT);

    p.draw_text(txui::Point(cx + 20.0, y1 + 48.0), "Virtual-1 / eDP-1", TXT_SEC, 0.88);
    p.draw_text(txui::Point(cx + 20.0, y1 + 72.0), "1920 x 1080 @ 60.00 Hz (Wayland Native Output)", TXT_DIM, 0.82);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 102.0, cw - 40.0, 1.0), DIVIDER);

    // Row: Display Scale
    draw_label_and_inactive_badge(p, cx + 20.0, y1 + 122.0, "Display Scale", 1.0, true);

    const char* scale_labels[] = {"100%", "125%", "150%", "200%"};
    draw_segmented_track(p, cx + cw - 256.0, y1 + 116.0, 236.0, 28.0, scale_labels, 4, m_display_scale_idx, active_accent);

    // Card 2: Color & Refresh Controls
    float64 y2 = y1 + 184.0;
    draw_card(p, txui::Rect(cx, y2, cw, 160.0));

    // Night Light
    draw_label_and_inactive_badge(p, cx + 20.0, y2 + 20.0, "Night Light", 1.0, true);
    draw_toggle(p, cx + cw - 56.0, y2 + 20.0, m_night_light_enabled, active_accent);
    p.draw_text(txui::Point(cx + 20.0, y2 + 48.0), "Warmer screen temperature to reduce eye strain at night", TXT_DIM, 0.82);

    p.fill_rect(txui::Rect(cx + 20.0, y2 + 78.0, cw - 40.0, 1.0), DIVIDER);

    // Variable Refresh Rate (VRR)
    draw_label_and_inactive_badge(p, cx + 20.0, y2 + 98.0, "Variable Refresh Rate (VRR)", 1.0, true);
    draw_toggle(p, cx + cw - 56.0, y2 + 98.0, m_vrr_enabled, active_accent);
    p.draw_text(txui::Point(cx + 20.0, y2 + 126.0), "Synchronizes refresh rate with graphics rendering (Adaptive Sync)", TXT_DIM, 0.82);
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
    float64 y1 = area.y() + 94.0;

    // Card 1: Accent Color
    draw_card(p, txui::Rect(cx, y1, cw, 132.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "System Accent Color", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y1 + 48.0), "Applies highlight tint and focus rings to desktop controls", TXT_DIM, 0.84);

    for (int i = 0; i < ACCENT_COUNT; ++i) {
        float64 sx = cx + 24.0 + static_cast<double>(i) * 54.0;
        float64 sy = y1 + 92.0;
        txui::Color col(ACCENT_PALETTE[i].r, ACCENT_PALETTE[i].g, ACCENT_PALETTE[i].b, 255);

        if (m_selected_accent_idx == i) {
            p.draw_circle(txui::Point(sx, sy), 17.0, 2.0, active_accent);
        }
        p.fill_circle(txui::Point(sx, sy), 13.0, col);
        if (m_selected_accent_idx == i) {
            p.fill_circle(txui::Point(sx, sy), 4.0, txui::Color(255, 255, 255, 255));
        }
    }

    // Card 2: Theme Mode
    float64 y2 = y1 + 152.0;
    draw_card(p, txui::Rect(cx, y2, cw, 82.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 20.0), "Interface Theme", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y2 + 48.0), "Dark theme active across all Tinexus platform modules", TXT_DIM, 0.84);

    draw_badge_pill(p, cx + cw - 120.0, y2 + 28.0, "Dark (Active)", SUCCESS_BG, SUCCESS_TXT);

    // Card 3: Wallpaper
    float64 y3 = y2 + 102.0;
    draw_card(p, txui::Rect(cx, y3, cw, 188.0));
    draw_label_and_inactive_badge(p, cx + 20.0, y3 + 20.0, "Desktop Wallpaper", 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y3 + 48.0), "Select from bundled wallpapers or /usr/share/backgrounds", TXT_DIM, 0.84);

    size_t count = std::min(size_t(4), m_wallpapers.size());
    float64 gap = 16.0;
    float64 total_gaps = gap * static_cast<double>(count > 1 ? count - 1 : 0);
    float64 card_w = (count > 0) ? std::clamp((cw - 40.0 - total_gaps) / static_cast<double>(count), 120.0, 260.0) : 150.0;

    for (size_t i = 0; i < count; ++i) {
        float64 wx = cx + 20.0 + static_cast<double>(i) * (card_w + gap);
        float64 wy = y3 + 76.0;
        bool sel = (m_selected_wallpaper_idx == static_cast<int>(i));

        if (sel) {
            p.fill_rounded_rect(txui::Rect(wx - 2.5, wy - 2.5, card_w + 5.0, 75.0), 8.0, active_accent);
        }

        p.fill_rounded_rect(txui::Rect(wx, wy, card_w, 70.0), 6.0,
            txui::Color(m_wallpapers[i].preview_r, m_wallpapers[i].preview_g, m_wallpapers[i].preview_b, 255));

        p.draw_text(txui::Point(wx + 4.0, wy + 78.0), m_wallpapers[i].name,
                    sel ? TXT_PRI : TXT_SEC, 0.82, sel);
    }
}

// ── 3. Page: Network (macOS Tahoe/Sonoma Style Wi-Fi & Adapters) ─────────────
void SettingsWidget::paint_network_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    draw_page_header(p, area, "Network",
                     "Wi-Fi networks, live signal quality, adapter states, and IP routing",
                     SettingsPage::Network);

    float64 cw = std::min(area.width() - 40.0, 760.0);
    float64 cx = area.x() + (area.width() - cw) * 0.5;
    float64 y1 = area.y() + 94.0;

    auto& wm = WifiManager::instance();
    bool wifi_on = wm.is_wifi_enabled();
    std::string conn_ssid = wm.get_connected_ssid();
    std::string ip_addr = wm.get_ip_address();
    std::string status_msg = wm.get_status_message();
    bool is_connecting = wm.is_connecting();
    std::string connecting_ssid = wm.get_connecting_ssid();
    bool is_scanning = wm.is_scanning();

    // ── Card 1: Master Wi-Fi Switch Card ──────────────────────────────────────
    float64 card1_h = 76.0;
    draw_card(p, txui::Rect(cx, y1, cw, card1_h));

    // Wi-Fi Icon Badge
    p.fill_circle(txui::Point(cx + 32.0, y1 + 38.0), 16.0, wifi_on ? active_accent : txui::Color(44, 44, 58, 255));
    draw_wifi_signal_bars(p, cx + 21.0, y1 + 30.0, wifi_on ? 4 : 1, txui::Color(255, 255, 255, 255));

    p.draw_text(txui::Point(cx + 60.0, y1 + 20.0), "Wi-Fi", TXT_PRI, 1.15, true);
    std::string wifi_subtitle;
    if (!wifi_on) {
        wifi_subtitle = "Wi-Fi is turned off";
    } else if (!conn_ssid.empty()) {
        wifi_subtitle = "Connected to \"" + conn_ssid + "\" (" + (ip_addr.empty() ? "Obtaining IP..." : ip_addr) + ")";
    } else if (is_connecting) {
        wifi_subtitle = "Connecting to \"" + connecting_ssid + "\"...";
    } else {
        wifi_subtitle = "Not Connected — Choose a network below";
    }
    p.draw_text(txui::Point(cx + 60.0, y1 + 46.0), wifi_subtitle, wifi_on ? TXT_SEC : TXT_DIM, 0.88);

    // Refresh / Scan Button (if Wi-Fi is ON)
    if (wifi_on) {
        float64 scan_w = 80.0;
        float64 scan_x = cx + cw - 60.0 - scan_w - 16.0;
        float64 scan_y = y1 + 24.0;
        m_wifi_scan_btn_rect = txui::Rect(scan_x, scan_y, scan_w, 28.0);
        txui::Color scan_bg = m_wifi_scan_hovered ? txui::Color(55, 55, 75, 255) : txui::Color(38, 38, 52, 220);
        p.fill_rounded_rect(m_wifi_scan_btn_rect, 6.0, scan_bg);
        std::string scan_label = is_scanning ? "Scanning..." : "Scan";
        float64 slw = estimate_text_width(scan_label, 0.82, true);
        p.draw_text(txui::Point(scan_x + (scan_w - slw) * 0.5, scan_y + 7.0),
                    scan_label, is_scanning ? active_accent : TXT_PRI, 0.82, true);
    } else {
        m_wifi_scan_btn_rect = txui::Rect(0, 0, 0, 0);
    }

    // Toggle Switch on far right
    float64 toggle_x = cx + cw - 56.0;
    float64 toggle_y = y1 + 27.0;
    m_wifi_toggle_rect = txui::Rect(toggle_x - 4.0, toggle_y - 4.0, 48.0, 30.0);
    draw_toggle(p, toggle_x, toggle_y, wifi_on, active_accent);

    // ── Card 2: Connected Network Details (if connected & Wi-Fi ON) ───────────
    float64 current_y = y1 + card1_h + 16.0;
    if (wifi_on && !conn_ssid.empty()) {
        float64 card_conn_h = 104.0;
        draw_card(p, txui::Rect(cx, current_y, cw, card_conn_h));

        // Row 1: Green active badge + SSID on left, Disconnect button on right
        draw_badge_pill(p, cx + 20.0, current_y + 18.0, "Connected", SUCCESS_BG, SUCCESS_TXT, 0.80);
        p.draw_text(txui::Point(cx + 120.0, current_y + 18.0), conn_ssid, TXT_PRI, 1.1, true);

        // Disconnect button on right
        float64 disc_w = 92.0;
        float64 disc_x = cx + cw - 20.0 - disc_w;
        float64 disc_y = current_y + 16.0;
        m_wifi_disconnect_btn_rect = txui::Rect(disc_x, disc_y, disc_w, 28.0);
        txui::Color disc_bg = m_wifi_disconnect_hovered ? DANGER_BG : txui::Color(44, 44, 58, 220);
        p.fill_rounded_rect(m_wifi_disconnect_btn_rect, 6.0, disc_bg);
        p.draw_text(txui::Point(disc_x + 14.0, disc_y + 7.0), "Disconnect",
                    m_wifi_disconnect_hovered ? DANGER_TXT : TXT_SEC, 0.82, true);

        p.fill_rect(txui::Rect(cx + 20.0, current_y + 54.0, cw - 40.0, 1.0), DIVIDER);

        // Row 2: Left: IP Address & Interface | Right: Security label & Signal meter
        std::string active_iface = wm.get_active_interface();
        std::string ip_label = "IP Address: " + (ip_addr.empty() ? "Configuring..." : ip_addr) + "   •   Interface: " + (active_iface.empty() ? "Wi-Fi" : active_iface);
        p.draw_text(txui::Point(cx + 20.0, current_y + 68.0), ip_label, TXT_SEC, 0.84);

        float64 sig_x = cx + cw - 40.0;
        int conn_bars = wm.get_connected_signal_bars();
        if (conn_bars == 0) conn_bars = 4;
        draw_wifi_signal_bars(p, sig_x, current_y + 68.0, conn_bars, SUCCESS_TXT);

        std::string sec_label = "WPA2/WPA3";
        float64 sec_w = estimate_text_width(sec_label, 0.80, false);
        p.draw_text(txui::Point(sig_x - sec_w - 12.0, current_y + 68.0), sec_label, TXT_DIM, 0.80);

        current_y += card_conn_h + 16.0;
    } else {
        m_wifi_disconnect_btn_rect = txui::Rect(0, 0, 0, 0);
    }

    // ── Card 3: Available Networks List (if Wi-Fi ON) ─────────────────────────
    if (wifi_on) {
        auto networks = wm.get_networks();
        float64 item_h = 46.0;
        size_t max_visible = std::min(size_t(6), networks.size());
        float64 list_header_h = 42.0;
        float64 card_list_h = list_header_h + (max_visible > 0 ? (static_cast<double>(max_visible) * item_h) : 52.0) + 12.0;

        draw_card(p, txui::Rect(cx, current_y, cw, card_list_h));

        // Header label
        p.draw_text(txui::Point(cx + 20.0, current_y + 16.0), "Known & Available Networks", TXT_PRI, 0.95, true);
        std::string count_str = std::to_string(networks.size()) + " networks detected";
        p.draw_text(txui::Point(cx + 230.0, current_y + 18.0), count_str, TXT_DIM, 0.80);

        p.fill_rect(txui::Rect(cx + 20.0, current_y + list_header_h, cw - 40.0, 1.0), DIVIDER);

        m_network_item_rects.clear();
        float64 ny = current_y + list_header_h + 6.0;

        if (networks.empty()) {
            std::string empty_msg = is_scanning ? "Scanning for available wireless networks..." : "No networks detected in range. Click 'Scan' above.";
            p.draw_text(txui::Point(cx + 20.0, ny + 14.0), empty_msg, TXT_DIM, 0.88);
        } else {
            for (size_t i = 0; i < max_visible; ++i) {
                const auto& net = networks[i];
                txui::Rect item_rect(cx + 8.0, ny, cw - 16.0, item_h);
                m_network_item_rects.push_back(item_rect);

                bool hovered = (m_hovered_network_idx == static_cast<int>(i));
                if (hovered) {
                    p.fill_rounded_rect(item_rect, 7.0, txui::Color(44, 44, 62, 160));
                }

                // 1. Signal Bars (1..4 bars based on RSSI!)
                draw_wifi_signal_bars(p, cx + 22.0, ny + 14.0, net.signal_bars,
                                      net.is_connected ? SUCCESS_TXT : TXT_PRI);

                // 2. Network SSID
                p.draw_text(txui::Point(cx + 52.0, ny + 14.0), net.ssid,
                            net.is_connected ? SUCCESS_TXT : TXT_PRI, 0.96, net.is_connected);

                // 3. Band badge (2.4 GHz vs 5 GHz)
                std::string band_str = (net.frequency_mhz >= 5000) ? "5 GHz" : "2.4 GHz";
                float64 band_x = cx + cw - 195.0;
                draw_badge_pill(p, band_x, ny + 13.0, band_str, txui::Color(32, 32, 44, 200), TXT_DIM, 0.72);

                // 4. Security Lock Icon (aligned in column before band badge)
                if (net.is_secured) {
                    draw_lock_icon(p, band_x - 22.0, ny + 17.0, TXT_DIM);
                }

                // 5. Status / Action on Right
                if (net.is_connected) {
                    draw_badge_pill(p, cx + cw - 105.0, ny + 13.0, "Connected", SUCCESS_BG, SUCCESS_TXT, 0.78);
                } else if (is_connecting && connecting_ssid == net.ssid) {
                    draw_badge_pill(p, cx + cw - 115.0, ny + 13.0, "Connecting...", WARNING_BG, WARNING_TXT, 0.78);
                } else {
                    float64 btn_w = 72.0;
                    float64 btn_h = 24.0;
                    float64 btn_x = cx + cw - 18.0 - btn_w;
                    float64 btn_y = ny + 11.0;
                    txui::Color btn_bg = hovered ? active_accent : txui::Color(44, 44, 58, 180);
                    p.fill_rounded_rect(txui::Rect(btn_x, btn_y, btn_w, btn_h), 5.0, btn_bg);
                    float64 clw = estimate_text_width("Connect", 0.78, true);
                    p.draw_text(txui::Point(btn_x + (btn_w - clw) * 0.5, btn_y + 4.0), "Connect",
                                hovered ? txui::Color(255, 255, 255, 255) : TXT_SEC, 0.78, true);
                }

                ny += item_h;
            }
        }

        current_y += card_list_h + 16.0;
    } else {
        m_network_item_rects.clear();
    }

    // ── Card 4: Hardware Interfaces & Ethernet (Physical Adapters) ────────────
    float64 iface_card_h = 92.0;
    draw_card(p, txui::Rect(cx, current_y, cw, iface_card_h));
    p.draw_text(txui::Point(cx + 20.0, current_y + 16.0), "Physical Network Adapters", TXT_PRI, 0.95, true);

    std::string ifaces_summary = "Active adapters: ";
    if (m_network_ifaces.empty()) {
        ifaces_summary += "None detected";
    } else {
        for (size_t i = 0; i < m_network_ifaces.size(); ++i) {
            bool is_wifi = (m_network_ifaces[i].name.rfind("wl", 0) == 0);
            std::string state_info = m_network_ifaces[i].ip4_addr.empty() ? m_network_ifaces[i].state : m_network_ifaces[i].ip4_addr;
            ifaces_summary += std::string(is_wifi ? "[Wi-Fi] " : "[Ethernet] ") + m_network_ifaces[i].name + " (" + state_info + ")";
            if (i + 1 < m_network_ifaces.size()) ifaces_summary += "  •  ";
        }
    }
    p.draw_text(txui::Point(cx + 20.0, current_y + 44.0), ifaces_summary, TXT_SEC, 0.88);
    p.draw_text(txui::Point(cx + 20.0, current_y + 68.0), "DNS Resolver: udhcpc active  •  Core daemons: strict offline-first boundary", TXT_DIM, 0.80);
}

// ── macOS-Style Wi-Fi Password Connect Modal Dialog ──────────────────────────
void SettingsWidget::paint_wifi_modal(txui::Painter& p) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    // 1. Semi-transparent backdrop scrim over the whole window
    p.fill_rect(frame(), txui::Color(0, 0, 0, 175));

    // 2. Centered Modal Card (width 450, height 260)
    float64 mw = 450.0;
    float64 mh = 260.0;
    float64 mx = frame().x() + (frame().width() - mw) * 0.5;
    float64 my = frame().y() + (frame().height() - mh) * 0.5;
    m_wifi_modal_rect = txui::Rect(mx, my, mw, mh);

    // Outer subtle border
    p.fill_rounded_rect(txui::Rect(mx - 1.0, my - 1.0, mw + 2.0, mh + 2.0), 13.0, txui::Color(65, 65, 90, 200));
    p.fill_gradient_rounded_rect(m_wifi_modal_rect, 12.0, txui::Color(32, 32, 46, 255), txui::Color(22, 22, 32, 255));

    // Lock Icon Badge (40x40) with icon centered at (40-14)/2 = 13.0
    float64 icon_x = mx + 24.0;
    float64 icon_y = my + 24.0;
    p.fill_rounded_rect(txui::Rect(icon_x, icon_y, 40.0, 40.0), 9.0, active_accent);
    draw_lock_icon(p, icon_x + 13.0, icon_y + 13.0, txui::Color(255, 255, 255, 255));

    // Modal Title & Subtitle
    std::string title_text = "Join \"" + m_wifi_modal_ssid + "\"";
    p.draw_text(txui::Point(mx + 76.0, my + 24.0), title_text, TXT_PRI, 1.15, true);
    p.draw_text(txui::Point(mx + 76.0, my + 50.0), "Enter the WPA2/WPA3 password for this network.", TXT_SEC, 0.85);

    // Password Input Field
    float64 fx = mx + 24.0;
    float64 fy = my + 92.0;
    float64 fw = mw - 48.0;
    float64 fh = 36.0;
    m_wifi_modal_input_rect = txui::Rect(fx, fy, fw, fh);

    // Outer focus glow (active accent)
    p.fill_rounded_rect(txui::Rect(fx - 1.0, fy - 1.0, fw + 2.0, fh + 2.0), 7.0, active_accent);
    p.fill_rounded_rect(txui::Rect(fx, fy, fw, fh), 6.0, txui::Color(18, 18, 26, 255));

    // Render password characters (either bullets or visible) with horizontal scrolling
    float64 eye_w = 64.0;
    float64 max_text_w = fw - eye_w - 24.0;

    std::string display_pw;
    if (m_wifi_modal_show_password) {
        display_pw = m_wifi_modal_password;
    } else {
        display_pw = std::string(m_wifi_modal_password.size(), '*');
    }

    size_t scroll_offset = 0;
    std::string visible_pw = display_pw;
    while (!visible_pw.empty() && estimate_text_width(visible_pw, 0.95, true) > max_text_w) {
        visible_pw.erase(0, 1);
        scroll_offset++;
    }
    m_wifi_modal_scroll_offset = scroll_offset;

    if (display_pw.empty()) {
        p.draw_text(txui::Point(fx + 12.0, fy + 9.0), "Password", TXT_DIM, 0.95);
        p.fill_rect(txui::Rect(fx + 14.0, fy + 8.0, 1.5, 20.0), active_accent);
    } else {
        p.draw_text(txui::Point(fx + 12.0, fy + 9.0), visible_pw, TXT_PRI, 0.95, true);
        float64 cursor_x = fx + 12.0 + estimate_text_width(visible_pw, 0.95, true);
        p.fill_rect(txui::Rect(cursor_x + 2.0, fy + 8.0, 1.5, 20.0), active_accent);
    }

    // Show Password toggle button on right of field
    float64 eye_x = fx + fw - eye_w - 6.0;
    float64 eye_y = fy + 6.0;
    m_wifi_modal_eye_btn = txui::Rect(eye_x, eye_y, eye_w, 24.0);
    p.fill_rounded_rect(m_wifi_modal_eye_btn, 4.0, m_wifi_modal_eye_hovered ? txui::Color(45, 45, 60, 220) : txui::Color(30, 30, 42, 200));
    std::string eye_label = m_wifi_modal_show_password ? "Hide" : "Show";
    p.draw_text(txui::Point(eye_x + 16.0, eye_y + 5.0), eye_label, TXT_SEC, 0.78, true);

    // Error message (if any)
    if (!m_wifi_modal_error.empty()) {
        p.draw_text(txui::Point(fx, fy + 44.0), m_wifi_modal_error, DANGER_TXT, 0.82);
    }

    // Action Buttons at bottom
    float64 btn_h = 32.0;
    float64 btn_w = 96.0;
    float64 btn_y = my + mh - btn_h - 20.0;

    // Cancel Button
    float64 cancel_x = mx + mw - 24.0 - (btn_w * 2.0 + 12.0);
    m_wifi_modal_cancel_btn = txui::Rect(cancel_x, btn_y, btn_w, btn_h);
    txui::Color cancel_bg = m_wifi_modal_cancel_hovered ? txui::Color(55, 55, 75, 255) : txui::Color(44, 44, 58, 220);
    p.fill_rounded_rect(m_wifi_modal_cancel_btn, 6.0, cancel_bg);
    p.draw_text(txui::Point(cancel_x + 26.0, btn_y + 8.0), "Cancel", TXT_PRI, 0.88, true);

    // Connect Button
    float64 conn_x = mx + mw - 24.0 - btn_w;
    m_wifi_modal_connect_btn = txui::Rect(conn_x, btn_y, btn_w, btn_h);
    bool can_connect = !m_wifi_modal_password.empty();
    txui::Color conn_bg = can_connect ? (m_wifi_modal_connect_hovered ? txui::Color(active_accent.r(), active_accent.g(), active_accent.b(), 220) : active_accent)
                                      : txui::Color(44, 44, 58, 140);
    p.fill_rounded_rect(m_wifi_modal_connect_btn, 6.0, conn_bg);
    p.draw_text(txui::Point(conn_x + 22.0, btn_y + 8.0), "Connect",
                can_connect ? txui::Color(255, 255, 255, 255) : TXT_DIM, 0.88, true);
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
    float64 y1 = area.y() + 94.0;

    // Card 1: Power & Display Sleep
    draw_card(p, txui::Rect(cx, y1, cw, 172.0));

    // Row 1: Screen timeout
    draw_label_and_inactive_badge(p, cx + 20.0, y1 + 20.0, "Turn off display after", 0.95, true);
    p.fill_rounded_rect(txui::Rect(cx + cw - 130.0, y1 + 16.0, 30.0, 24.0), 5.0, txui::Color(34, 34, 46, 220));
    p.draw_text(txui::Point(cx + cw - 120.0, y1 + 20.0), "-", TXT_PRI, 1.1, true);
    std::string to_str = std::to_string(m_screen_timeout_min) + " min";
    p.draw_text(txui::Point(cx + cw - 90.0, y1 + 21.0), to_str, TXT_PRI, 0.9);
    p.fill_rounded_rect(txui::Rect(cx + cw - 42.0, y1 + 16.0, 30.0, 24.0), 5.0, txui::Color(34, 34, 46, 220));
    p.draw_text(txui::Point(cx + cw - 32.0, y1 + 20.0), "+", TXT_PRI, 1.1, true);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 56.0, cw - 40.0, 1.0), DIVIDER);

    // Row 2: Sleep after
    draw_label_and_inactive_badge(p, cx + 20.0, y1 + 76.0, "Put system to sleep after", 0.95, true);
    p.fill_rounded_rect(txui::Rect(cx + cw - 130.0, y1 + 72.0, 30.0, 24.0), 5.0, txui::Color(34, 34, 46, 220));
    p.draw_text(txui::Point(cx + cw - 120.0, y1 + 76.0), "-", TXT_PRI, 1.1, true);
    std::string sl_str = std::to_string(m_sleep_after_min) + " min";
    p.draw_text(txui::Point(cx + cw - 90.0, y1 + 77.0), sl_str, TXT_PRI, 0.9);
    p.fill_rounded_rect(txui::Rect(cx + cw - 42.0, y1 + 72.0, 30.0, 24.0), 5.0, txui::Color(34, 34, 46, 220));
    p.draw_text(txui::Point(cx + cw - 32.0, y1 + 76.0), "+", TXT_PRI, 1.1, true);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 112.0, cw - 40.0, 1.0), DIVIDER);

    // Row 3: Energy Mode
    p.draw_text(txui::Point(cx + 20.0, y1 + 130.0), "Energy Mode", TXT_PRI, 0.95, true);
    const char* profiles[] = {"Power Saver", "Balanced", "Performance"};
    draw_segmented_track(p, cx + cw - 324.0, y1 + 124.0, 304.0, 28.0, profiles, 3, m_power_profile_idx, active_accent);

    // Card 2: Clipboard Subsystem
    float64 y2 = y1 + 192.0;
    draw_card(p, txui::Rect(cx, y2, cw, 136.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 20.0), "Clipboard Subsystem", TXT_PRI, 1.0, true);
    float64 clip_w = estimate_text_width("Clipboard Subsystem", 1.0, true);
    draw_badge_pill(p, cx + 20.0 + clip_w + 12.0, y2 + 18.0, "Active (Live)", SUCCESS_BG, SUCCESS_TXT);
    p.draw_text(txui::Point(cx + 20.0, y2 + 48.0), "Live Wayland clipboard is active. History daemon (tinexus-clip) in Phase 2.", TXT_DIM, 0.82);

    p.fill_rect(txui::Rect(cx + 20.0, y2 + 78.0, cw - 40.0, 1.0), DIVIDER);

    p.draw_text(txui::Point(cx + 20.0, y2 + 98.0), "History Buffer Size:", TXT_SEC, 0.88);
    const char* caps[] = {"25 items", "50 items", "100 items"};
    int cap_idx = (m_clipboard_history_size <= 25 ? 0 : (m_clipboard_history_size <= 50 ? 1 : 2));
    draw_segmented_track(p, cx + cw - 228.0, y2 + 92.0, 208.0, 28.0, caps, 3, cap_idx, active_accent);

    // Card 3: Session Actions (Lock, Sleep, Restart, Shut Down)
    float64 y3 = y2 + 156.0;
    draw_card(p, txui::Rect(cx, y3, cw, 96.0));
    p.draw_text(txui::Point(cx + 20.0, y3 + 18.0), "Session Actions", TXT_PRI, 1.0, true);

    struct BtnDef { const char* label; txui::Color bg; };
    BtnDef btns[] = {
        {"Lock Screen", txui::Color(44, 44, 60, 255)},
        {"Sleep",       txui::Color(44, 44, 60, 255)},
        {"Restart",     txui::Color(215, 120, 30, 255)},
        {"Shut Down",   txui::Color(225, 55, 55, 255)}
    };

    float64 btn_gap = 14.0;
    float64 btn_w = std::clamp((cw - 40.0 - 3.0 * btn_gap) / 4.0, 110.0, 220.0);
    for (int i = 0; i < 4; ++i) {
        float64 bx = cx + 20.0 + static_cast<double>(i) * (btn_w + btn_gap);
        p.fill_rounded_rect(txui::Rect(bx, y3 + 46.0, btn_w, 32.0), 7.0, btns[i].bg);
        float64 lbl_w = estimate_text_width(btns[i].label, 0.88, true);
        p.draw_text(txui::Point(bx + std::max(8.0, (btn_w - lbl_w) * 0.5), y3 + 55.0),
                    btns[i].label, txui::Color(255, 255, 255, 255), 0.88, true);
    }

    if (!m_session_status_msg.empty()) {
        p.draw_text(txui::Point(cx + 20.0, y3 + 86.0), m_session_status_msg, active_accent, 0.82);
    }
}

// ── 5. Page: Keyboard Shortcuts ──────────────────────────────────────────────
void SettingsWidget::paint_keyboard_shortcuts_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    draw_page_header(p, area, "Keyboard Shortcuts",
                     "System-wide global hotkeys and window management bindings",
                     SettingsPage::KeyboardShortcuts);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 94.0;

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

    draw_card(p, txui::Rect(cx, y1, cw, 326.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "Global System Bindings", TXT_PRI, 1.05, true);

    for (int i = 0; i < 9; ++i) {
        float64 sy = y1 + 54.0 + static_cast<double>(i) * 28.0;
        p.draw_text(txui::Point(cx + 20.0, sy + 5.0), shortcuts[i].action, TXT_SEC, 0.88);

        float64 kx = cx + cw - 150.0;
        if (shortcuts[i].mod[0] != '\0') {
            draw_keycap(p, kx, sy, shortcuts[i].mod);
            p.draw_text(txui::Point(kx + 50.0, sy + 5.0), "+", TXT_DIM, 0.9, true);
            draw_keycap(p, kx + 64.0, sy, shortcuts[i].key);
        } else {
            draw_keycap(p, kx + 40.0, sy, shortcuts[i].key);
        }

        if (i < 8) {
            p.fill_rect(txui::Rect(cx + 20.0, sy + 27.0, cw - 40.0, 1.0), txui::Color(28, 28, 38, 160));
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
    float64 y1 = area.y() + 94.0;

    // Card 1: Authentication Options
    draw_card(p, txui::Rect(cx, y1, cw, 158.0));

    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "Require password when waking from sleep", TXT_PRI, 0.95, true);
    draw_toggle(p, cx + cw - 56.0, y1 + 20.0, m_lock_on_sleep, active_accent);
    p.draw_text(txui::Point(cx + 20.0, y1 + 48.0), "Invokes tinexus-lock on system resume", TXT_DIM, 0.82);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 78.0, cw - 40.0, 1.0), DIVIDER);

    draw_label_and_inactive_badge(p, cx + 20.0, y1 + 98.0, "Enable PAM Biometric Authentication", 0.95, true);
    draw_toggle(p, cx + cw - 56.0, y1 + 98.0, m_pam_auth, active_accent);
    p.draw_text(txui::Point(cx + 20.0, y1 + 126.0), "Fingerprint and smartcard login via PAM modules", TXT_DIM, 0.82);

    // Card 2: Application Gatekeeper
    float64 y2 = y1 + 178.0;
    draw_card(p, txui::Rect(cx, y2, cw, 158.0));

    p.draw_text(txui::Point(cx + 20.0, y2 + 20.0), "Application Gatekeeper", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y2 + 48.0), "Scans /opt/tinexus-apps for cryptographically verified application bundles", TXT_DIM, 0.82);

    m_rescan_btn_rect = txui::Rect(cx + cw - 106.0, y2 + 20.0, 86.0, 26.0);
    p.fill_rounded_rect(m_rescan_btn_rect, 6.0, m_rescan_hovered ? txui::Color(44, 44, 60, 255) : txui::Color(32, 32, 44, 255));
    p.draw_text(txui::Point(m_rescan_btn_rect.x() + 14.0, m_rescan_btn_rect.y() + 6.0), "Re-scan", TXT_PRI, 0.84, true);

    p.fill_rect(txui::Rect(cx + 20.0, y2 + 78.0, cw - 40.0, 1.0), DIVIDER);

    if (m_unverified_apps.empty()) {
        p.fill_circle(txui::Point(cx + 34.0, y2 + 108.0), 8.0, txui::Color(72, 205, 120, 255));
        p.fill_circle(txui::Point(cx + 34.0, y2 + 108.0), 4.0, txui::Color(16, 40, 24, 255));
        p.draw_text(txui::Point(cx + 52.0, y2 + 98.0),
                    "No unverified apps found in /opt/tinexus-apps",
                    TXT_PRI, 0.98, true);
        p.draw_text(txui::Point(cx + 52.0, y2 + 120.0),
                    "All scanned binary packages match trusted cryptographic signatures.",
                    TXT_SEC, 0.85);
    } else {
        float64 uy = y2 + 84.0;
        for (const auto& app : m_unverified_apps) {
            p.draw_text(txui::Point(cx + 20.0, uy), app.name, WARNING_TXT, 0.9, true);
            std::string hash_prev = "SHA-256: " + app.hash.substr(0, std::min(size_t(16), app.hash.size())) + "...";
            p.draw_text(txui::Point(cx + 20.0, uy + 18.0), hash_prev, TXT_DIM, 0.8);

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
    float64 y1 = area.y() + 94.0;

    // Card 1: System Specs
    draw_card(p, txui::Rect(cx, y1, cw, 204.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "System Specifications", TXT_PRI, 1.05, true);

    struct SpecRow { const char* label; const std::string& val; };
    std::string arch = "x86_64 (Wayland Native)";
    SpecRow specs[] = {
        {"Operating System",     m_os_version},
        {"Processor",            m_cpu_model},
        {"System Memory",        m_mem_info},
        {"Platform Architecture", arch},
        {"Platform Target",       m_comp_info}
    };

    for (int i = 0; i < 5; ++i) {
        float64 sy = y1 + 54.0 + static_cast<double>(i) * 28.0;
        p.draw_text(txui::Point(cx + 20.0, sy), specs[i].label, TXT_SEC, 0.88);
        p.draw_text(txui::Point(cx + 200.0, sy), specs[i].val, TXT_PRI, 0.88, (i == 0));
        if (i < 4) {
            p.fill_rect(txui::Rect(cx + 20.0, sy + 20.0, cw - 40.0, 1.0), txui::Color(28, 28, 38, 160));
        }
    }

    // Card 2: Legal & Architecture (High Contrast)
    float64 y2 = y1 + 224.0;
    draw_card(p, txui::Rect(cx, y2, cw, 104.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 20.0), "Tinexus Platform", TXT_PRI, 1.05, true);
    p.draw_text(txui::Point(cx + 20.0, y2 + 48.0),
                "Wayland-native Linux desktop platform powered by wlroots, Vulkan, and libtxui.",
                TXT_SEC, 0.85);
    p.draw_text(txui::Point(cx + 20.0, y2 + 72.0),
                "Licensed under GPL-2.0-or-later. Designed with simplicity.",
                txui::Color(175, 175, 195, 240), 0.84);

    // Card 3: Storage & System Firmware (Real statvfs + /sys/firmware reads)
    float64 y3 = y2 + 124.0;
    draw_card(p, txui::Rect(cx, y3, cw, 104.0));
    p.draw_text(txui::Point(cx + 20.0, y3 + 20.0), "Storage & System Firmware", TXT_PRI, 1.05, true);

    std::string storage_str = "Root Filesystem: Scanning...";
    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0) {
        uint64_t total_mb = (vfs.f_blocks * vfs.f_frsize) / (1024 * 1024);
        uint64_t free_mb  = (vfs.f_bavail * vfs.f_frsize) / (1024 * 1024);
        uint64_t used_mb  = (total_mb > free_mb) ? (total_mb - free_mb) : 0;
        storage_str = "Root Overlay: " + std::to_string(used_mb) + " MB used of " +
                      std::to_string(total_mb) + " MB (" + std::to_string(free_mb) + " MB free)";
    }
    p.draw_text(txui::Point(cx + 20.0, y3 + 48.0), storage_str, TXT_SEC, 0.88);

    bool is_uefi = std::filesystem::exists("/sys/firmware/efi");
    std::string fw_str = is_uefi ? "Boot Environment: UEFI 64-bit (BOOTX64.EFI Active)"
                                 : "Boot Environment: BIOS / MBR Hybrid (eltorito.img Active)";
    p.draw_text(txui::Point(cx + 20.0, y3 + 72.0), fw_str, TXT_DIM, 0.84);
}

} // namespace tinexus::settings_ui
