#include "settings/SettingsWidget.hpp"
#include <txui/render/FontMetrics.hpp>
#include <txui/input/Event.hpp>
#include <common/NetUtils.hpp>
#include <common/AudioUtils.hpp>
#include <common/BacklightUtils.hpp>
#include <common/DisplayUtils.hpp>
#include "settings/WifiManager.hpp"
#include <guard/crypto_validator.hpp>

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <cmath>

using namespace txui;

namespace tinexus::settings_ui {

// ── Color Palette & macOS Tokens ─────────────────────────────────────────────
static const txui::Color BG_DARK      (14, 14, 20, 255);
static const txui::Color SIDEBAR_BG   (19, 19, 27, 255);
static const txui::Color CARD_TOP     (26, 26, 38, 255);
static const txui::Color CARD_BOT     (20, 20, 30, 255);
static const txui::Color TXT_PRI      (245, 245, 252, 255);
static const txui::Color TXT_SEC      (165, 165, 185, 255);
static const txui::Color TXT_DIM      (105, 105, 125, 255);
static const txui::Color DIVIDER      (38, 38, 52, 255);
static const txui::Color BORDER_SUBTLE(48, 48, 66, 180);

static const txui::Color SUCCESS_BG   (30, 70, 45, 220);
static const txui::Color SUCCESS_TXT  (80, 220, 120, 255);
static const txui::Color WARNING_BG   (75, 55, 20, 220);
static const txui::Color WARNING_TXT  (240, 180, 50, 255);
static const txui::Color DANGER_BG    (85, 25, 25, 220);
static const txui::Color DANGER_TXT   (255, 90, 90, 255);

// ── Accent Palette Options ───────────────────────────────────────────────────
const tinexus::settings_ui::AccentOption ACCENT_PALETTE[] = {
    {0,   195, 255, "Electric Cyan"},
    {0,   122, 255, "macOS Blue"},
    {88,  86,  214, "Deep Violet"},
    {255, 45,  85,  "Hot Pink"},
    {255, 149, 0,   "Vibrant Orange"},
    {52,  199, 89,  "Mint Green"}
};
constexpr int ACCENT_COUNT = sizeof(ACCENT_PALETTE) / sizeof(ACCENT_PALETTE[0]);

// ── Shared UI Utilities ──────────────────────────────────────────────────────
inline void draw_badge_pill(txui::Painter& p, float64 x, float64 y,
                            const std::string& text, const txui::Color& bg,
                            const txui::Color& fg, float64 font_size = 11.0) {
    txui::Badge::render(p, txui::Point(x, y), text, bg, fg, font_size, 4.0, 7.0, 2.5);
}

inline void draw_inactive_badge(txui::Painter& p, float64 x, float64 y, float64 font_size = 10.0) {
    draw_badge_pill(p, x, y, "Not yet active", WARNING_BG, WARNING_TXT, font_size);
}

inline void draw_label_and_inactive_badge(
    txui::Painter& p, float64 x, float64 y,
    const std::string& label, float64 font_size = 13.0, bool bold = true
) {
    p.draw_text(txui::Point(x, y), label, TXT_PRI, font_size, bold);
    float64 text_w = txui::FontMetrics::measure(label, font_size).width;
    float64 badge_x = x + text_w + 10.0;
    float64 by = y - 1.0;
    draw_inactive_badge(p, badge_x, by, 9.5);
}

inline void draw_card(txui::Painter& p, const txui::Rect& rect) {
    p.fill_gradient_rounded_rect(rect, 10.0, CARD_TOP, CARD_BOT);
}

inline void draw_keycap(txui::Painter& p, float64 x, float64 y, const std::string& key) {
    float64 tw = txui::FontMetrics::measure(key, 11.0).width;
    float64 w = std::max(26.0, tw + 14.0);
    p.fill_rounded_rect(txui::Rect(x, y, w, 22.0), 5.0, txui::Color(44, 44, 58, 255));
    p.fill_rounded_rect(txui::Rect(x + 1.0, y + 1.0, w - 2.0, 19.0), 4.0, txui::Color(32, 32, 42, 255));
    p.draw_text(txui::Point(x + (w - tw) * 0.5, y + 4.0), key, TXT_PRI, 11.0, true);
}

// ── Category Icon Badge ──────────────────────────────────────────────────────
static void draw_icon_badge_base(txui::Painter& p, float64 x, float64 y,
                                 const txui::Color& top_c, const txui::Color& bot_c,
                                 float64 size = 20.0, float64 radius = 5.0) {
    p.fill_rounded_rect(txui::Rect(x - 0.5, y + 1.0, size + 1.0, size + 1.0), radius + 0.5, txui::Color(0, 0, 0, 75));
    p.fill_gradient_rounded_rect(txui::Rect(x, y, size, size), radius, top_c, bot_c);
}

// ── Constructor ─────────────────────────────────────────────────────────────
SettingsWidget::SettingsWidget() {
    read_compositor_info();
    scan_wallpapers();
    scan_network_ifaces();
    load_config();
    init_child_widgets();
    update_accent_styling();
}

void SettingsWidget::init_child_widgets() {
    // 1. Sidebar Navigation Items (8 items)
    m_nav_items.clear();
    const struct NavDef { const char* label; SettingsPage page; } nav_defs[] = {
        {"Display",            SettingsPage::Display},
        {"Sound",              SettingsPage::Sound},
        {"Personalization",    SettingsPage::Personalization},
        {"Network",            SettingsPage::Network},
        {"System & Power",     SettingsPage::System},
        {"Keyboard Shortcuts", SettingsPage::KeyboardShortcuts},
        {"Privacy & Security", SettingsPage::PrivacySecurity},
        {"About",              SettingsPage::About}
    };

    for (const auto& def : nav_defs) {
        auto item = make_ref<NavItem>(
            def.label,
            [this, page = def.page](Painter& p, const Rect& r) {
                switch (page) {
                    case SettingsPage::Display:           draw_icon_display(p, r.x(), r.y()); break;
                    case SettingsPage::Sound:             draw_icon_sound(p, r.x(), r.y()); break;
                    case SettingsPage::Personalization:   draw_icon_personalization(p, r.x(), r.y()); break;
                    case SettingsPage::Network:           draw_icon_network(p, r.x(), r.y()); break;
                    case SettingsPage::System:            draw_icon_system(p, r.x(), r.y()); break;
                    case SettingsPage::KeyboardShortcuts: draw_icon_keyboard(p, r.x(), r.y()); break;
                    case SettingsPage::PrivacySecurity:   draw_icon_privacy(p, r.x(), r.y()); break;
                    case SettingsPage::About:             draw_icon_about(p, r.x(), r.y()); break;
                }
            },
            [this, page = def.page]() {
                select_page(page);
            }
        );
        item->set_selected(def.page == m_current_page);
        m_nav_items.push_back(item);
    }

    // 2. Display Page Widgets
    m_display_scale_control = make_ref<SegmentedControl>(
        std::vector<std::string>{"100%", "125%", "150%", "200%"},
        static_cast<size_t>(m_display_scale_idx)
    );
    m_display_scale_control->set_on_segment_selected([this](size_t idx) {
        m_display_scale_idx = static_cast<int>(idx);
        save_config();
        mark_needs_paint();
    });

    m_brightness_slider = make_ref<Slider>(
        0.0, 100.0, static_cast<double>(hardware::BacklightUtils::get_brightness_percent())
    );
    m_brightness_slider->set_on_value_changed([this](double val) {
        hardware::BacklightUtils::set_brightness_percent(static_cast<int>(val), /*persist=*/false, /*throttle=*/true);
        mark_needs_paint();
    });

    m_night_light_toggle = make_ref<ToggleSwitch>(m_night_light_enabled, [this](bool checked) {
        m_night_light_enabled = checked;
        save_config();
        mark_needs_paint();
    });

    m_vrr_toggle = make_ref<ToggleSwitch>(m_vrr_enabled, [this](bool checked) {
        m_vrr_enabled = checked;
        save_config();
        mark_needs_paint();
    });

    // 2b. Sound Page Widgets
    m_volume_slider = make_ref<Slider>(
        0.0, 100.0, static_cast<double>(hardware::AudioUtils::get_volume_percent())
    );
    m_volume_slider->set_on_value_changed([this](double val) {
        int v = static_cast<int>(val);
        hardware::AudioUtils::set_volume_percent(v, /*persist=*/false, /*throttle=*/true);
        if (m_sound_mute_toggle) {
            m_sound_mute_toggle->set_checked(hardware::AudioUtils::is_muted());
        }
        mark_needs_paint();
    });

    m_sound_mute_toggle = make_ref<ToggleSwitch>(
        hardware::AudioUtils::is_muted(),
        [this](bool checked) {
            if (checked != hardware::AudioUtils::is_muted()) {
                hardware::AudioUtils::toggle_mute(/*persist=*/true);
                mark_needs_paint();
            }
        }
    );

    m_sound_test_btn = make_ref<Button>("Play Test Sound", []() {
        hardware::AudioUtils::play_chime();
    });
    m_sound_test_btn->set_style(Button::Style::Standard);

    // 3. Personalization Page Widgets
    std::vector<Color> swatch_colors;
    for (int i = 0; i < ACCENT_COUNT; ++i) {
        swatch_colors.emplace_back(ACCENT_PALETTE[i].r, ACCENT_PALETTE[i].g, ACCENT_PALETTE[i].b, 255);
    }
    m_accent_picker = make_ref<ColorPicker>(
        swatch_colors,
        static_cast<size_t>(m_selected_accent_idx),
        [this](size_t idx, Color) {
            m_selected_accent_idx = static_cast<int>(idx);
            update_accent_styling();
            save_config();
            mark_needs_paint();
        }
    );

    m_theme_toggle_btn = make_ref<Button>(
        m_theme_mode == "Dark" ? "Dark Mode" : "Light Mode",
        [this]() {
            m_theme_mode = (m_theme_mode == "Dark") ? "Light" : "Dark";
            m_theme_toggle_btn->set_text(m_theme_mode == "Dark" ? "Dark Mode" : "Light Mode");
            save_config();
            mark_needs_paint();
        }
    );
    m_theme_toggle_btn->set_style(Button::Style::Standard);

    // 4. Network Page Widgets
    m_wifi_master_toggle = make_ref<ToggleSwitch>(
        WifiManager::instance().is_wifi_enabled(),
        [this](bool checked) {
            WifiManager::instance().set_wifi_enabled(checked);
            mark_needs_paint();
        }
    );

    m_wifi_scan_btn = make_ref<Button>("Scan Networks", [this]() {
        WifiManager::instance().trigger_scan();
        mark_needs_paint();
    });
    m_wifi_scan_btn->set_style(Button::Style::Standard);

    m_wifi_disconnect_btn = make_ref<Button>("Disconnect", [this]() {
        WifiManager::instance().disconnect();
        mark_needs_paint();
    });
    m_wifi_disconnect_btn->set_style(Button::Style::Danger);

    // 5. Wi-Fi Password Modal Widgets
    m_wifi_password_input = make_ref<TextInput>("Enter network password...");
    m_wifi_password_input->set_secure_mode(true);

    m_wifi_modal_eye_btn = make_ref<Button>("Show", [this]() {
        bool sec = m_wifi_password_input->is_secure_mode();
        m_wifi_password_input->set_secure_mode(!sec);
        m_wifi_modal_eye_btn->set_text(!sec ? "Hide" : "Show");
        mark_needs_paint();
    });
    m_wifi_modal_eye_btn->set_style(Button::Style::Ghost);

    m_wifi_modal_connect_btn = make_ref<Button>("Connect", [this]() {
        std::string pw = m_wifi_password_input->text();
        if (pw.empty()) {
            m_wifi_modal_error = "Password cannot be empty";
            mark_needs_paint();
        } else {
            WifiManager::instance().connect(m_wifi_modal_ssid, pw);
            m_wifi_modal_open = false;
            m_wifi_password_input->set_text("");
            m_wifi_modal_error.clear();
            mark_needs_paint();
        }
    });
    m_wifi_modal_connect_btn->set_style(Button::Style::Primary);

    m_wifi_modal_cancel_btn = make_ref<Button>("Cancel", [this]() {
        m_wifi_modal_open = false;
        m_wifi_password_input->set_text("");
        m_wifi_modal_error.clear();
        mark_needs_paint();
    });
    m_wifi_modal_cancel_btn->set_style(Button::Style::Ghost);

    // 6. System Page Widgets
    m_timeout_slider = make_ref<Slider>(1.0, 60.0, static_cast<double>(m_screen_timeout_min));
    m_timeout_slider->set_on_value_changed([this](double val) {
        m_screen_timeout_min = std::max(1, static_cast<int>(val));
        save_config();
        mark_needs_paint();
    });

    m_sleep_slider = make_ref<Slider>(5.0, 120.0, static_cast<double>(m_sleep_after_min));
    m_sleep_slider->set_on_value_changed([this](double val) {
        m_sleep_after_min = std::max(5, static_cast<int>(val));
        save_config();
        mark_needs_paint();
    });

    m_power_profile_control = make_ref<SegmentedControl>(
        std::vector<std::string>{"Power Saver", "Balanced", "Performance"},
        static_cast<size_t>(m_power_profile_idx)
    );
    m_power_profile_control->set_on_segment_selected([this](size_t idx) {
        m_power_profile_idx = static_cast<int>(idx);
        save_config();
        mark_needs_paint();
    });

    size_t clip_idx = (m_clipboard_history_size <= 25) ? 0 : ((m_clipboard_history_size <= 50) ? 1 : 2);
    m_clipboard_size_control = make_ref<SegmentedControl>(
        std::vector<std::string>{"25 items", "50 items", "100 items"},
        clip_idx
    );
    m_clipboard_size_control->set_on_segment_selected([this](size_t idx) {
        m_clipboard_history_size = (idx == 0 ? 25 : (idx == 1 ? 50 : 100));
        save_config();
        mark_needs_paint();
    });

    m_lock_sleep_toggle = make_ref<ToggleSwitch>(m_lock_on_sleep, [this](bool checked) {
        m_lock_on_sleep = checked;
        save_config();
        mark_needs_paint();
    });

    m_pam_auth_toggle = make_ref<ToggleSwitch>(m_pam_auth, [this](bool checked) {
        m_pam_auth = checked;
        save_config();
        mark_needs_paint();
    });

    m_session_lock_btn     = make_ref<Button>("Lock Screen", [this]() { trigger_session_action(0); });
    m_session_lock_btn->set_style(Button::Style::Standard);
    m_session_suspend_btn  = make_ref<Button>("Sleep",       [this]() { trigger_session_action(1); });
    m_session_suspend_btn->set_style(Button::Style::Standard);
    m_session_reboot_btn   = make_ref<Button>("Restart",     [this]() { trigger_session_action(2); });
    m_session_reboot_btn->set_style(Button::Style::Standard);
    m_session_shutdown_btn = make_ref<Button>("Shut Down",   [this]() { trigger_session_action(3); });
    m_session_shutdown_btn->set_style(Button::Style::Danger);

    // 7. Privacy & Security Page Widgets
    m_rescan_btn = make_ref<Button>("Re-scan", [this]() {
        refresh_unverified_apps();
        mark_needs_paint();
    });
    m_rescan_btn->set_style(Button::Style::Standard);
}

void SettingsWidget::update_accent_styling() noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_col(current_accent.r, current_accent.g, current_accent.b, 255);

    for (auto& item : m_nav_items) {
        item->set_accent_color(active_col);
    }
    if (m_brightness_slider)     m_brightness_slider->set_active_color(active_col);
    if (m_night_light_toggle)    m_night_light_toggle->set_active_color(active_col);
    if (m_vrr_toggle)            m_vrr_toggle->set_active_color(active_col);
    if (m_volume_slider)         m_volume_slider->set_active_color(active_col);
    if (m_sound_mute_toggle)     m_sound_mute_toggle->set_active_color(active_col);
    if (m_wifi_master_toggle)    m_wifi_master_toggle->set_active_color(active_col);
    if (m_timeout_slider)        m_timeout_slider->set_active_color(active_col);
    if (m_sleep_slider)          m_sleep_slider->set_active_color(active_col);
    if (m_lock_sleep_toggle)     m_lock_sleep_toggle->set_active_color(active_col);
    if (m_pam_auth_toggle)       m_pam_auth_toggle->set_active_color(active_col);
}

void SettingsWidget::open_wifi_password_modal(const std::string& ssid) {
    m_wifi_modal_open = true;
    m_wifi_modal_ssid = ssid;
    m_wifi_modal_error.clear();
    m_wifi_password_input->set_text("");
    m_wifi_password_input->set_secure_mode(true);
    m_wifi_password_input->set_focused(true);
    m_wifi_modal_eye_btn->set_text("Show");
    mark_needs_layout();
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
    m_comp_info = hardware::DisplayUtils::get_compositor_version_string();
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
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;

        if (line.starts_with("accent_index")) {
            try {
                m_selected_accent_idx = std::stoi(line.substr(pos + 1));
                if (m_selected_accent_idx < 0 || m_selected_accent_idx >= ACCENT_COUNT) {
                    m_selected_accent_idx = 0;
                }
            } catch (...) {}
        } else if (line.starts_with("theme_mode")) {
            std::string mode = line.substr(pos + 1);
            mode.erase(0, mode.find_first_not_of(" \t\""));
            mode.erase(mode.find_last_not_of(" \t\"") + 1);
            if (!mode.empty()) m_theme_mode = mode;
        } else if (line.starts_with("display_scale_idx")) {
            try {
                m_display_scale_idx = std::clamp(std::stoi(line.substr(pos + 1)), 0, 3);
            } catch (...) {}
        } else if (line.starts_with("night_light")) {
            m_night_light_enabled = (line.find("true") != std::string::npos);
        } else if (line.starts_with("vrr_enabled")) {
            m_vrr_enabled = (line.find("true") != std::string::npos);
        } else if (line.starts_with("screen_timeout_min")) {
            try {
                m_screen_timeout_min = std::clamp(std::stoi(line.substr(pos + 1)), 1, 60);
            } catch (...) {}
        } else if (line.starts_with("sleep_after_min")) {
            try {
                m_sleep_after_min = std::clamp(std::stoi(line.substr(pos + 1)), 5, 120);
            } catch (...) {}
        } else if (line.starts_with("power_profile_idx")) {
            try {
                m_power_profile_idx = std::clamp(std::stoi(line.substr(pos + 1)), 0, 2);
            } catch (...) {}
        } else if (line.starts_with("lock_on_sleep")) {
            m_lock_on_sleep = (line.find("true") != std::string::npos);
        } else if (line.starts_with("pam_auth")) {
            m_pam_auth = (line.find("true") != std::string::npos);
        } else if (line.starts_with("clipboard_history_size")) {
            try {
                m_clipboard_history_size = std::stoi(line.substr(pos + 1));
            } catch (...) {}
        }
    }
}

void SettingsWidget::save_config() {
    try {
        const char* xdg_config = std::getenv("XDG_CONFIG_HOME");
        std::string config_dir = xdg_config ? xdg_config : (std::string(std::getenv("HOME") ? std::getenv("HOME") : "/root") + "/.config");
        std::error_code ec;
        std::filesystem::create_directories(config_dir + "/tinexus", ec);
        std::string config_path = config_dir + "/tinexus/settings.toml";

        std::string tmp_path = config_path + ".tmp";
        std::ofstream out(tmp_path);
        if (!out.is_open()) return;

        out << "# Tinexus Desktop Settings Configuration\n";
        out << "accent_index = " << m_selected_accent_idx << "\n";
        out << "theme_mode = \"" << m_theme_mode << "\"\n";
        out << "display_scale_idx = " << m_display_scale_idx << "\n";
        out << "night_light = " << (m_night_light_enabled ? "true" : "false") << "\n";
        out << "vrr_enabled = " << (m_vrr_enabled ? "true" : "false") << "\n";
        out << "screen_timeout_min = " << m_screen_timeout_min << "\n";
        out << "sleep_after_min = " << m_sleep_after_min << "\n";
        out << "power_profile_idx = " << m_power_profile_idx << "\n";
        out << "lock_on_sleep = " << (m_lock_on_sleep ? "true" : "false") << "\n";
        out << "pam_auth = " << (m_pam_auth ? "true" : "false") << "\n";
        out << "clipboard_history_size = " << m_clipboard_history_size << "\n";
        out.close();

        std::filesystem::rename(tmp_path, config_path, ec);
    } catch (...) {}
}

void SettingsWidget::refresh_unverified_apps() {
    m_unverified_apps.clear();
    try {
        std::string app_dir = "/opt/tinexus-apps";
        std::error_code ec;
        if (!std::filesystem::exists(app_dir, ec) || ec) return;

        for (const auto& entry : std::filesystem::directory_iterator(app_dir, ec)) {
            if (ec) break;
            if (!entry.is_regular_file(ec) || ec) continue;
            std::string path = entry.path().string();
            if (path.ends_with(".sig")) continue;

            std::string sig_path = path + ".sig";
            bool verified = false;

            int bin_fd = open(path.c_str(), O_RDONLY);
            if (bin_fd >= 0) {
                std::string hash = tinexus::guard::CryptoValidator::compute_sha256_fd(bin_fd);
                close(bin_fd);

                if (!hash.empty()) {
                    if (std::filesystem::exists(sig_path, ec) && !ec) {
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
    } catch (...) {}
}

void SettingsWidget::trust_app(const std::string& hash) {
    try {
        std::string trust_path = "/var/lib/tinexus/trust-overrides.conf";
        std::error_code ec;
        std::filesystem::create_directories("/var/lib/tinexus", ec);
        std::ofstream out(trust_path, std::ios::app);
        if (out.is_open()) {
            out << hash << "\n";
            out.close();
        }
        refresh_unverified_apps();
    } catch (...) {}
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
    mark_needs_paint();
}

void SettingsWidget::select_page(SettingsPage page) {
    m_current_page = page;
    for (size_t i = 0; i < m_nav_items.size(); ++i) {
        m_nav_items[i]->set_selected(static_cast<int>(page) == static_cast<int>(i));
    }
    if (m_current_page == SettingsPage::PrivacySecurity) {
        refresh_unverified_apps();
    } else if (m_current_page == SettingsPage::Network) {
        scan_network_ifaces();
    }
    layout(frame());
    mark_needs_paint();
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

    // Sidebar navigation items layout
    double nav_y = f.y() + 74.0;
    for (auto& item : m_nav_items) {
        item->layout(txui::Rect(f.x() + 10.0, nav_y, SIDEBAR_W - 20.0, 40.0));
        nav_y += 44.0;
    }

    // Content area geometry
    txui::Rect area(m_content_rect.x() + 32.0, m_content_rect.y() + 28.0,
                    m_content_rect.width() - 64.0, m_content_rect.height() - 56.0);
    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 94.0;

    // 1. Display Page Widgets
    if (m_display_scale_control) {
        m_display_scale_control->layout(txui::Rect(cx + cw - 264.0, y1 + 114.0, 244.0, 30.0));
    }
    float64 disp_y2 = y1 + 176.0;
    if (m_brightness_slider) {
        m_brightness_slider->layout(txui::Rect(cx + 20.0, disp_y2 + 46.0, cw - 40.0, 24.0));
    }
    float64 disp_y3 = disp_y2 + 96.0;
    if (m_night_light_toggle) {
        m_night_light_toggle->layout(txui::Rect(cx + cw - 56.0, disp_y3 + 18.0, 44.0, 24.0));
    }
    if (m_vrr_toggle) {
        m_vrr_toggle->layout(txui::Rect(cx + cw - 56.0, disp_y3 + 96.0, 44.0, 24.0));
    }

    // 1b. Sound Page Widgets
    if (m_volume_slider) {
        m_volume_slider->layout(txui::Rect(cx + 20.0, y1 + 52.0, cw - 40.0, 24.0));
    }
    if (m_sound_test_btn) {
        m_sound_test_btn->layout(txui::Rect(cx + 20.0, y1 + 96.0, 150.0, 32.0));
    }
    if (m_sound_mute_toggle) {
        m_sound_mute_toggle->layout(txui::Rect(cx + cw - 56.0, y1 + 100.0, 44.0, 24.0));
    }

    // 2. Personalization Page Widgets
    if (m_accent_picker) {
        m_accent_picker->layout(txui::Rect(cx + 20.0, y1 + 48.0, cw - 40.0, 36.0));
    }
    float64 pers_y2 = y1 + 116.0;
    if (m_theme_toggle_btn) {
        m_theme_toggle_btn->layout(txui::Rect(cx + cw - 130.0, pers_y2 + 20.0, 110.0, 32.0));
    }

    // 3. Network Page Widgets
    if (m_wifi_master_toggle) {
        m_wifi_master_toggle->layout(txui::Rect(cx + cw - 56.0, y1 + 24.0, 44.0, 24.0));
    }
    if (m_wifi_scan_btn) {
        m_wifi_scan_btn->layout(txui::Rect(cx + cw - 190.0, y1 + 22.0, 120.0, 30.0));
    }
    if (m_wifi_disconnect_btn) {
        m_wifi_disconnect_btn->layout(txui::Rect(cx + cw - 320.0, y1 + 22.0, 115.0, 30.0));
    }

    // 4. System Page Widgets
    if (m_timeout_slider) {
        m_timeout_slider->layout(txui::Rect(cx + cw - 260.0, y1 + 14.0, 170.0, 28.0));
    }
    if (m_sleep_slider) {
        m_sleep_slider->layout(txui::Rect(cx + cw - 260.0, y1 + 70.0, 170.0, 28.0));
    }
    if (m_power_profile_control) {
        m_power_profile_control->layout(txui::Rect(cx + cw - 330.0, y1 + 124.0, 310.0, 30.0));
    }
    float64 sys_y2 = y1 + 192.0;
    if (m_clipboard_size_control) {
        m_clipboard_size_control->layout(txui::Rect(cx + cw - 240.0, sys_y2 + 92.0, 220.0, 30.0));
    }
    float64 sys_y3 = sys_y2 + 156.0;
    float64 btn_gap = 12.0;
    float64 btn_w = std::clamp((cw - 40.0 - 3.0 * btn_gap) / 4.0, 90.0, 200.0);
    if (m_session_lock_btn)     m_session_lock_btn->layout(txui::Rect(cx + 20.0, sys_y3 + 46.0, btn_w, 34.0));
    if (m_session_suspend_btn)  m_session_suspend_btn->layout(txui::Rect(cx + 20.0 + (btn_w + btn_gap), sys_y3 + 46.0, btn_w, 34.0));
    if (m_session_reboot_btn)   m_session_reboot_btn->layout(txui::Rect(cx + 20.0 + 2.0 * (btn_w + btn_gap), sys_y3 + 46.0, btn_w, 34.0));
    if (m_session_shutdown_btn) m_session_shutdown_btn->layout(txui::Rect(cx + 20.0 + 3.0 * (btn_w + btn_gap), sys_y3 + 46.0, btn_w, 34.0));

    // 5. Privacy & Security Page Widgets
    if (m_lock_sleep_toggle) {
        m_lock_sleep_toggle->layout(txui::Rect(cx + cw - 56.0, y1 + 20.0, 44.0, 24.0));
    }
    if (m_pam_auth_toggle) {
        m_pam_auth_toggle->layout(txui::Rect(cx + cw - 56.0, y1 + 98.0, 44.0, 24.0));
    }
    float64 priv_y2 = y1 + 178.0;
    if (m_rescan_btn) {
        m_rescan_btn->layout(txui::Rect(cx + cw - 106.0, priv_y2 + 18.0, 86.0, 28.0));
    }

    // 6. Wi-Fi Password Modal Dialog Widgets
    float64 mw = 460.0;
    float64 mh = 260.0;
    float64 mx = f.x() + (f.width() - mw) * 0.5;
    float64 my = f.y() + (f.height() - mh) * 0.5;
    m_wifi_modal_rect = txui::Rect(mx, my, mw, mh);

    if (m_wifi_password_input) {
        m_wifi_password_input->layout(txui::Rect(mx + 28.0, my + 94.0, mw - 120.0, 38.0));
    }
    if (m_wifi_modal_eye_btn) {
        m_wifi_modal_eye_btn->layout(txui::Rect(mx + mw - 85.0, my + 94.0, 58.0, 38.0));
    }
    if (m_wifi_modal_cancel_btn) {
        m_wifi_modal_cancel_btn->layout(txui::Rect(mx + mw - 224.0, my + mh - 54.0, 92.0, 34.0));
    }
    if (m_wifi_modal_connect_btn) {
        m_wifi_modal_connect_btn->layout(txui::Rect(mx + mw - 120.0, my + mh - 54.0, 94.0, 34.0));
    }
}

// ── Event Handler ────────────────────────────────────────────────────────────
bool SettingsWidget::handle_event(const txui::Event& event) noexcept {
    // 1. Wi-Fi Password Modal Interception
    if (m_wifi_modal_open) {
        if (event.type == txui::EventType::KeyDown) {
            if (event.keyboard.key == txui::Key::Escape) {
                m_wifi_modal_open = false;
                m_wifi_password_input->set_text("");
                m_wifi_modal_error.clear();
                mark_needs_paint();
                return true;
            }
            if (event.keyboard.key == txui::Key::Enter) {
                std::string pw = m_wifi_password_input->text();
                if (pw.empty()) {
                    m_wifi_modal_error = "Password cannot be empty";
                } else {
                    WifiManager::instance().connect(m_wifi_modal_ssid, pw);
                    m_wifi_modal_open = false;
                    m_wifi_password_input->set_text("");
                    m_wifi_modal_error.clear();
                }
                mark_needs_paint();
                return true;
            }
        }

        if (m_wifi_password_input && m_wifi_password_input->handle_event(event)) return true;
        if (m_wifi_modal_eye_btn && m_wifi_modal_eye_btn->handle_event(event))   return true;
        if (m_wifi_modal_cancel_btn && m_wifi_modal_cancel_btn->handle_event(event)) return true;
        if (m_wifi_modal_connect_btn && m_wifi_modal_connect_btn->handle_event(event)) return true;

        // Block clicks from passing through modal scrim
        if (event.type == txui::EventType::PointerButtonPress ||
            event.type == txui::EventType::PointerButtonRelease) {
            return true;
        }
        return true;
    }

    // 2. Sidebar Navigation Items
    for (auto& item : m_nav_items) {
        if (item->handle_event(event)) {
            return true;
        }
    }

    // 3. Active Page Specific Dispatch
    switch (m_current_page) {
        case SettingsPage::Display:
            if (m_display_scale_control && m_display_scale_control->handle_event(event)) return true;
            if (m_brightness_slider && m_brightness_slider->handle_event(event)) {
                if (event.type == txui::EventType::PointerButtonRelease) {
                    hardware::BacklightUtils::set_brightness_percent(
                        static_cast<int>(m_brightness_slider->value()), /*persist=*/true, /*throttle=*/false
                    );
                }
                return true;
            }
            if (m_night_light_toggle && m_night_light_toggle->handle_event(event))       return true;
            if (m_vrr_toggle && m_vrr_toggle->handle_event(event))                       return true;
            break;

        case SettingsPage::Sound:
            if (m_volume_slider && m_volume_slider->handle_event(event)) {
                if (event.type == txui::EventType::PointerButtonRelease) {
                    hardware::AudioUtils::set_volume_percent(
                        static_cast<int>(m_volume_slider->value()), /*persist=*/true, /*throttle=*/false
                    );
                }
                return true;
            }
            if (m_sound_mute_toggle && m_sound_mute_toggle->handle_event(event)) return true;
            if (m_sound_test_btn && m_sound_test_btn->handle_event(event)) return true;
            break;

        case SettingsPage::Personalization:
            if (m_accent_picker && m_accent_picker->handle_event(event))         return true;
            if (m_theme_toggle_btn && m_theme_toggle_btn->handle_event(event))   return true;
            if (handle_wallpaper_event(event)) return true;
            break;

        case SettingsPage::Network:
            if (m_wifi_master_toggle && m_wifi_master_toggle->handle_event(event)) return true;
            if (m_wifi_scan_btn && m_wifi_scan_btn->handle_event(event))           return true;
            if (m_wifi_disconnect_btn && m_wifi_disconnect_btn->handle_event(event)) return true;
            if (handle_network_list_event(event)) return true;
            break;

        case SettingsPage::System:
            if (m_timeout_slider && m_timeout_slider->handle_event(event))             return true;
            if (m_sleep_slider && m_sleep_slider->handle_event(event))                 return true;
            if (m_power_profile_control && m_power_profile_control->handle_event(event)) return true;
            if (m_clipboard_size_control && m_clipboard_size_control->handle_event(event)) return true;
            if (m_lock_sleep_toggle && m_lock_sleep_toggle->handle_event(event))       return true;
            if (m_pam_auth_toggle && m_pam_auth_toggle->handle_event(event))           return true;
            if (m_session_lock_btn && m_session_lock_btn->handle_event(event))         return true;
            if (m_session_suspend_btn && m_session_suspend_btn->handle_event(event))   return true;
            if (m_session_reboot_btn && m_session_reboot_btn->handle_event(event))     return true;
            if (m_session_shutdown_btn && m_session_shutdown_btn->handle_event(event)) return true;
            break;

        case SettingsPage::PrivacySecurity:
            if (m_lock_sleep_toggle && m_lock_sleep_toggle->handle_event(event)) return true;
            if (m_pam_auth_toggle && m_pam_auth_toggle->handle_event(event))     return true;
            if (m_rescan_btn && m_rescan_btn->handle_event(event))               return true;
            if (handle_privacy_event(event)) return true;
            break;

        default:
            break;
    }

    return false;
}

bool SettingsWidget::handle_wallpaper_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerButtonPress && event.pointer.button == txui::MouseButton::Left) {
        txui::Rect area(m_content_rect.x() + 32.0, m_content_rect.y() + 28.0,
                        m_content_rect.width() - 64.0, m_content_rect.height() - 56.0);
        float64 cx = area.x();
        float64 y1 = area.y() + 94.0;
        float64 y3 = y1 + 116.0 + 86.0;
        float64 card_w = 110.0;
        float64 card_gap = 14.0;

        for (size_t i = 0; i < m_wallpapers.size(); ++i) {
            float64 wx = cx + 20.0 + static_cast<double>(i) * (card_w + card_gap);
            float64 wy = y3 + 52.0;
            txui::Rect wrect(wx, wy, card_w, 70.0);
            if (wrect.contains(txui::Point(event.pointer.x, event.pointer.y))) {
                m_selected_wallpaper_idx = static_cast<int>(i);
                mark_needs_paint();
                return true;
            }
        }
    }
    return false;
}

bool SettingsWidget::handle_network_list_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerMove) {
        int old_hover = m_hovered_network_idx;
        m_hovered_network_idx = -1;
        for (size_t i = 0; i < m_network_item_rects.size(); ++i) {
            if (m_network_item_rects[i].contains(txui::Point(event.pointer.x, event.pointer.y))) {
                m_hovered_network_idx = static_cast<int>(i);
                break;
            }
        }
        if (old_hover != m_hovered_network_idx) mark_needs_paint();
        return m_hovered_network_idx != -1;
    }

    if (event.type == txui::EventType::PointerButtonPress && event.pointer.button == txui::MouseButton::Left) {
        for (size_t i = 0; i < m_network_item_rects.size(); ++i) {
            if (m_network_item_rects[i].contains(txui::Point(event.pointer.x, event.pointer.y))) {
                const auto nets = WifiManager::instance().get_networks();
                if (i < nets.size()) {
                    if (nets[i].is_secured && !nets[i].is_connected) {
                        open_wifi_password_modal(nets[i].ssid);
                    } else if (!nets[i].is_secured) {
                        WifiManager::instance().connect(nets[i].ssid, "");
                    }
                }
                return true;
            }
        }
    }
    return false;
}

bool SettingsWidget::handle_privacy_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerButtonPress && event.pointer.button == txui::MouseButton::Left) {
        for (auto& app : m_unverified_apps) {
            if (app.btn_rect.contains(txui::Point(event.pointer.x, event.pointer.y))) {
                trust_app(app.hash);
                return true;
            }
        }
    }
    return false;
}

// ── Paint Pipeline ───────────────────────────────────────────────────────────
void SettingsWidget::paint_override(txui::Painter& p) const noexcept {
    // 1. Overall canvas background
    p.fill_rect(frame(), BG_DARK);

    // 2. Sidebar
    paint_sidebar(p);

    // 3. Vertical divider
    p.fill_rect(txui::Rect(m_sidebar_rect.right(), frame().y(), 1.0, frame().height()), DIVIDER);

    // 4. Content Page area
    txui::Rect area(m_content_rect.x() + 32.0, m_content_rect.y() + 28.0,
                    m_content_rect.width() - 64.0, m_content_rect.height() - 56.0);

    switch (m_current_page) {
        case SettingsPage::Display:           paint_display_page(p, area); break;
        case SettingsPage::Sound:             paint_sound_page(p, area); break;
        case SettingsPage::Personalization:   paint_personalization_page(p, area); break;
        case SettingsPage::Network:           paint_network_page(p, area); break;
        case SettingsPage::System:            paint_system_page(p, area); break;
        case SettingsPage::KeyboardShortcuts: paint_keyboard_shortcuts_page(p, area); break;
        case SettingsPage::PrivacySecurity:   paint_privacy_security_page(p, area); break;
        case SettingsPage::About:             paint_about_page(p, area); break;
    }

    // 5. Wi-Fi Connect Modal Overlay
    if (m_wifi_modal_open) {
        paint_wifi_modal(p);
    }
}

// ── Sidebar Painting ─────────────────────────────────────────────────────────
void SettingsWidget::paint_sidebar(txui::Painter& p) const noexcept {
    p.fill_rect(m_sidebar_rect, SIDEBAR_BG);

    // Title label (Window controls are rendered natively by txui::ChromeWidget titlebar)
    p.draw_text(txui::Point(m_sidebar_rect.x() + 18.0, m_sidebar_rect.y() + 42.0),
                "Settings", TXT_PRI, 15.0, true);

    // Sidebar navigation items
    for (const auto& item : m_nav_items) {
        item->paint(p);
    }
}

// ── Page Header Helper ───────────────────────────────────────────────────────
static void draw_page_header(txui::Painter& p, const txui::Rect& area,
                             const char* title, const char* subtitle,
                             SettingsPage /*page*/) {
    p.draw_text(txui::Point(area.x(), area.y()), title, TXT_PRI, 22.0, true);
    p.draw_text(txui::Point(area.x(), area.y() + 32.0), subtitle, TXT_SEC, 13.0);
    p.fill_rect(txui::Rect(area.x(), area.y() + 66.0, area.width(), 1.0), DIVIDER);
}

// ── 1. Page: Display ─────────────────────────────────────────────────────────
void SettingsWidget::paint_display_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    draw_page_header(p, area, "Display",
                     "Resolution, brightness, display scaling, Night Light, and refresh rates",
                     SettingsPage::Display);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 94.0;

    // Card 1: Display Information
    draw_card(p, txui::Rect(cx, y1, cw, 160.0));

    auto disp = hardware::DisplayUtils::get_primary_display();
    std::string disp_title = "Primary Display (" + disp.connector_name + ")";
    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), disp_title, TXT_PRI, 15.0, true);
    draw_badge_pill(p, cx + cw - 80.0, y1 + 18.0, "Primary", SUCCESS_BG, SUCCESS_TXT);
    p.draw_text(txui::Point(cx + 20.0, y1 + 48.0), disp.formatted_line, TXT_SEC, 12.0);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 78.0, cw - 40.0, 1.0), DIVIDER);

    p.draw_text(txui::Point(cx + 20.0, y1 + 98.0), "Display Scaling", TXT_PRI, 13.0, true);
    p.draw_text(txui::Point(cx + 20.0, y1 + 122.0), "Fractional scaling powered by Wayland wp-fractional-scale-v1", TXT_DIM, 11.5);
    if (m_display_scale_control) {
        m_display_scale_control->paint(p);
    }

    // Card 2: Display Backlight Brightness
    float64 y2 = y1 + 176.0;
    draw_card(p, txui::Rect(cx, y2, cw, 84.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 18.0), "Display Brightness", TXT_PRI, 13.0, true);
    int cur_bright = m_brightness_slider ? static_cast<int>(m_brightness_slider->value()) : hardware::BacklightUtils::get_brightness_percent();
    p.draw_text(txui::Point(cx + cw - 60.0, y2 + 18.0), std::to_string(cur_bright) + "%", TXT_PRI, 13.0, true);
    if (m_brightness_slider) {
        m_brightness_slider->paint(p);
    }

    // Card 3: Features
    float64 y3 = y2 + 96.0;
    draw_card(p, txui::Rect(cx, y3, cw, 160.0));

    p.draw_text(txui::Point(cx + 20.0, y3 + 20.0), "Night Light", TXT_PRI, 13.0, true);
    p.draw_text(txui::Point(cx + 20.0, y3 + 48.0), "Warmer screen colors reduce eye strain at night (3400K color temp)", TXT_DIM, 11.5);
    if (m_night_light_toggle) {
        m_night_light_toggle->paint(p);
    }

    p.fill_rect(txui::Rect(cx + 20.0, y3 + 78.0, cw - 40.0, 1.0), DIVIDER);

    p.draw_text(txui::Point(cx + 20.0, y3 + 98.0), "Adaptive Sync (VRR)", TXT_PRI, 13.0, true);
    p.draw_text(txui::Point(cx + 20.0, y3 + 126.0), "Variable refresh rate for tear-free gaming and low latency rendering", TXT_DIM, 11.5);
    if (m_vrr_toggle) {
        m_vrr_toggle->paint(p);
    }
}

// ── 1b. Page: Sound ──────────────────────────────────────────────────────────
void SettingsWidget::paint_sound_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    draw_page_header(p, area, "Sound",
                     "Audio output volume, hardware codecs, and sound playback test",
                     SettingsPage::Sound);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 94.0;

    // Card 1: Master Volume & Output Controls
    draw_card(p, txui::Rect(cx, y1, cw, 146.0));

    int cur_vol = m_volume_slider ? static_cast<int>(m_volume_slider->value()) : hardware::AudioUtils::get_volume_percent();
    bool muted = hardware::AudioUtils::is_muted() || (cur_vol == 0);
    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "Output Volume", TXT_PRI, 14.5, true);
    std::string vol_pct_str = muted ? "Muted" : (std::to_string(cur_vol) + "%");
    txui::Color vol_col = muted ? DANGER_TXT : TXT_PRI;
    p.draw_text(txui::Point(cx + cw - 66.0, y1 + 20.0), vol_pct_str, vol_col, 13.0, true);

    if (m_volume_slider) {
        m_volume_slider->paint(p);
    }

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 86.0, cw - 40.0, 1.0), DIVIDER);

    if (m_sound_test_btn) {
        m_sound_test_btn->paint(p);
    }

    p.draw_text(txui::Point(cx + cw - 160.0, y1 + 104.0), "Mute Audio Output", TXT_SEC, 12.0);
    if (m_sound_mute_toggle) {
        m_sound_mute_toggle->paint(p);
    }

    // Card 2: Output Hardware Devices
    float64 y2 = y1 + 162.0;
    draw_card(p, txui::Rect(cx, y2, cw, 180.0));

    p.draw_text(txui::Point(cx + 20.0, y2 + 20.0), "Detected Sound Hardware", TXT_PRI, 15.0, true);

    // Enumerate sound cards from /proc/asound/cards
    std::vector<std::string> sound_cards;
    std::ifstream asound_in("/proc/asound/cards");
    if (asound_in.is_open()) {
        std::string line;
        while (std::getline(asound_in, line)) {
            if (!line.empty() && std::isdigit(line[0])) {
                sound_cards.push_back(line);
            }
        }
    }

    if (!sound_cards.empty()) {
        draw_badge_pill(p, cx + cw - 90.0, y2 + 18.0, "ALSA Native", SUCCESS_BG, SUCCESS_TXT);

        std::string active_ctrl = hardware::AudioUtils::detect_primary_control();
        std::string dev_desc = "Primary Active Mixer Channel: [" + active_ctrl + "]  •  Direct ALSA Kernel Driver";
        p.draw_text(txui::Point(cx + 20.0, y2 + 48.0), dev_desc, TXT_SEC, 12.0);

        p.fill_rect(txui::Rect(cx + 20.0, y2 + 76.0, cw - 40.0, 1.0), DIVIDER);

        float64 card_y = y2 + 96.0;
        for (size_t i = 0; i < std::min<size_t>(sound_cards.size(), 2); ++i) {
            p.draw_text(txui::Point(cx + 20.0, card_y), sound_cards[i], TXT_PRI, 12.0, true);
            p.draw_text(txui::Point(cx + 20.0, card_y + 18.0), "Direct Hardware PCM Playback  •  48 kHz / 24-bit", TXT_DIM, 11.0);
            card_y += 38.0;
        }
    } else {
        draw_badge_pill(p, cx + cw - 105.0, y2 + 18.0, "Not Detected", WARNING_BG, WARNING_TXT);

        std::string dev_desc = "No active ALSA audio controller or soundcard found in /proc/asound/cards";
        p.draw_text(txui::Point(cx + 20.0, y2 + 48.0), dev_desc, TXT_SEC, 12.0);

        p.fill_rect(txui::Rect(cx + 20.0, y2 + 76.0, cw - 40.0, 1.0), DIVIDER);

        p.draw_text(txui::Point(cx + 20.0, y2 + 96.0), "No output devices detected", TXT_PRI, 13.0, true);
        p.draw_text(txui::Point(cx + 20.0, y2 + 118.0), "Audio driver or hardware codec initialization is pending.", TXT_DIM, 11.0);
    }
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
    draw_card(p, txui::Rect(cx, y1, cw, 96.0));
    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "System Accent Color", TXT_PRI, 14.5, true);
    if (m_accent_picker) {
        m_accent_picker->paint(p);
    }

    // Card 2: Appearance Mode
    float64 y2 = y1 + 116.0;
    draw_card(p, txui::Rect(cx, y2, cw, 70.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 24.0), "Theme Mode", TXT_PRI, 14.0, true);
    draw_badge_pill(p, cx + 130.0, y2 + 24.0, m_theme_mode + " (Active)", SUCCESS_BG, SUCCESS_TXT);
    if (m_theme_toggle_btn) {
        m_theme_toggle_btn->paint(p);
    }

    // Card 3: Wallpapers
    float64 y3 = y2 + 86.0;
    draw_card(p, txui::Rect(cx, y3, cw, 170.0));
    p.draw_text(txui::Point(cx + 20.0, y3 + 20.0), "Desktop Backdrop", TXT_PRI, 14.5, true);

    float64 card_w = 110.0;
    float64 card_gap = 14.0;
    for (size_t i = 0; i < m_wallpapers.size(); ++i) {
        float64 wx = cx + 20.0 + static_cast<double>(i) * (card_w + card_gap);
        float64 wy = y3 + 52.0;
        bool sel = (m_selected_wallpaper_idx == static_cast<int>(i));

        if (sel) {
            p.fill_rounded_rect(txui::Rect(wx - 2.5, wy - 2.5, card_w + 5.0, 75.0), 8.0, active_accent);
        }
        p.fill_rounded_rect(txui::Rect(wx, wy, card_w, 70.0), 6.0,
                            txui::Color(m_wallpapers[i].preview_r, m_wallpapers[i].preview_g, m_wallpapers[i].preview_b, 255));
        p.draw_text(txui::Point(wx + 8.0, wy + 78.0), m_wallpapers[i].name,
                    sel ? active_accent : TXT_SEC, 11.5, sel);
    }
}

// ── 3. Page: Network & Wi-Fi ─────────────────────────────────────────────────
void SettingsWidget::paint_network_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    draw_page_header(p, area, "Network & Wi-Fi",
                     "Wireless networks, wired interfaces, IP configuration, and security",
                     SettingsPage::Network);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 94.0;

    bool wifi_on = WifiManager::instance().is_wifi_enabled();

    // Card 1: Wi-Fi Master Control
    draw_card(p, txui::Rect(cx, y1, cw, 78.0));
    p.fill_circle(txui::Point(cx + 32.0, y1 + 38.0), 16.0, wifi_on ? active_accent : txui::Color(44, 44, 58, 255));
    draw_wifi_signal_bars(p, cx + 23.0, y1 + 30.0, 4, txui::Color(255, 255, 255, 255));

    p.draw_text(txui::Point(cx + 64.0, y1 + 22.0), "Wi-Fi Interface", TXT_PRI, 15.0, true);
    p.draw_text(txui::Point(cx + 64.0, y1 + 46.0),
                wifi_on ? "Enabled  •  nl80211 wireless subsystem active" : "Disabled  •  Hardware radio powered off",
                TXT_SEC, 11.5);

    if (m_wifi_disconnect_btn) m_wifi_disconnect_btn->paint(p);
    if (m_wifi_scan_btn)       m_wifi_scan_btn->paint(p);
    if (m_wifi_master_toggle)  m_wifi_master_toggle->paint(p);

    // Card 2: Available Networks
    float64 y2 = y1 + 98.0;
    const auto networks = WifiManager::instance().get_networks();
    float64 list_h = std::max(120.0, 60.0 + static_cast<double>(networks.size()) * 48.0);
    draw_card(p, txui::Rect(cx, y2, cw, list_h));

    p.draw_text(txui::Point(cx + 20.0, y2 + 20.0), "Available Networks", TXT_PRI, 14.5, true);

    m_network_item_rects.clear();
    float64 item_y = y2 + 52.0;

    if (networks.empty()) {
        std::string status_txt = WifiManager::instance().get_status_message();
        if (status_txt.empty() || status_txt == "Not Connected") {
            status_txt = WifiManager::instance().is_scanning() ? "Scanning for nearby Wi-Fi networks..." : "No Wi-Fi networks found. Click 'Scan Networks' to refresh.";
        }
        p.draw_text(txui::Point(cx + 20.0, y2 + 58.0), status_txt, TXT_SEC, 12.5);
    }

    for (size_t i = 0; i < networks.size(); ++i) {
        const auto& net = networks[i];
        txui::Rect nrect(cx + 12.0, item_y, cw - 24.0, 42.0);
        m_network_item_rects.push_back(nrect);

        bool hovered = (m_hovered_network_idx == static_cast<int>(i));
        if (hovered) {
            p.fill_rounded_rect(nrect, 6.0, txui::Color(44, 44, 62, 180));
        }

        draw_wifi_signal_bars(p, nrect.x() + 14.0, nrect.y() + 12.0, net.signal_bars,
                              net.is_connected ? active_accent : TXT_PRI);

        p.draw_text(txui::Point(nrect.x() + 46.0, nrect.y() + 12.0), net.ssid, TXT_PRI, 13.0, true);

        if (net.is_secured) {
            float64 ssid_w = txui::FontMetrics::measure(net.ssid, 13.0).width;
            draw_lock_icon(p, nrect.x() + 52.0 + ssid_w, nrect.y() + 14.0, TXT_DIM);
        }

        if (net.is_connected) {
            draw_badge_pill(p, cx + cw - 110.0, nrect.y() + 10.0, "Connected", SUCCESS_BG, SUCCESS_TXT);
        } else if (WifiManager::instance().is_connecting() && WifiManager::instance().get_connecting_ssid() == net.ssid) {
            draw_badge_pill(p, cx + cw - 120.0, nrect.y() + 10.0, "Connecting...", WARNING_BG, WARNING_TXT);
        }

        item_y += 46.0;
    }
}

// ── macOS-Style Wi-Fi Password Connect Modal Dialog ──────────────────────────
void SettingsWidget::paint_wifi_modal(txui::Painter& p) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    // 1. Semi-transparent backdrop scrim over whole canvas
    p.fill_rect(frame(), txui::Color(0, 0, 0, 185));

    // 2. Centered Modal Card
    p.fill_rounded_rect(txui::Rect(m_wifi_modal_rect.x() - 1.0, m_wifi_modal_rect.y() - 1.0,
                                   m_wifi_modal_rect.width() + 2.0, m_wifi_modal_rect.height() + 2.0),
                        13.0, BORDER_SUBTLE);
    p.fill_gradient_rounded_rect(m_wifi_modal_rect, 12.0, CARD_TOP, CARD_BOT);

    // 3. Lock Icon Badge
    float64 icon_x = m_wifi_modal_rect.x() + 26.0;
    float64 icon_y = m_wifi_modal_rect.y() + 24.0;
    p.fill_rounded_rect(txui::Rect(icon_x, icon_y, 42.0, 42.0), 9.0, active_accent);
    draw_lock_icon(p, icon_x + 14.0, icon_y + 14.0, txui::Color(255, 255, 255, 255));

    // 4. Modal Title & Subtitle
    std::string title_text = "Join \"" + m_wifi_modal_ssid + "\"";
    p.draw_text(txui::Point(m_wifi_modal_rect.x() + 80.0, m_wifi_modal_rect.y() + 26.0),
                title_text, TXT_PRI, 16.0, true);
    p.draw_text(txui::Point(m_wifi_modal_rect.x() + 80.0, m_wifi_modal_rect.y() + 52.0),
                "Enter WPA2/WPA3 password for this network", TXT_SEC, 12.0);

    // 5. Interactive Password Input & Eye-toggle
    if (m_wifi_password_input) {
        m_wifi_password_input->paint(p);
    }
    if (m_wifi_modal_eye_btn) {
        m_wifi_modal_eye_btn->paint(p);
    }

    // 6. Error message (if any)
    if (!m_wifi_modal_error.empty()) {
        p.draw_text(txui::Point(m_wifi_modal_rect.x() + 28.0, m_wifi_modal_rect.y() + 140.0),
                    m_wifi_modal_error, DANGER_TXT, 12.0);
    }

    // 7. Action Buttons
    if (m_wifi_modal_cancel_btn) {
        m_wifi_modal_cancel_btn->paint(p);
    }
    if (m_wifi_modal_connect_btn) {
        m_wifi_modal_connect_btn->paint(p);
    }
}

// ── 4. Page: System & Power ──────────────────────────────────────────────────
void SettingsWidget::paint_system_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    draw_page_header(p, area, "System & Power",
                     "Power management, timeout intervals, clipboard buffers, and session controls",
                     SettingsPage::System);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 94.0;

    // Card 1: Power & Display Sleep
    draw_card(p, txui::Rect(cx, y1, cw, 172.0));

    // Row 1: Screen timeout
    p.draw_text(txui::Point(cx + 20.0, y1 + 18.0), "Turn off display after", TXT_PRI, 13.5, true);
    std::string to_str = std::to_string(m_screen_timeout_min) + " min";
    p.draw_text(txui::Point(cx + cw - 70.0, y1 + 18.0), to_str, TXT_PRI, 12.5, true);
    if (m_timeout_slider) {
        m_timeout_slider->paint(p);
    }

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 56.0, cw - 40.0, 1.0), DIVIDER);

    // Row 2: Sleep after
    p.draw_text(txui::Point(cx + 20.0, y1 + 74.0), "Put system to sleep after", TXT_PRI, 13.5, true);
    std::string sl_str = std::to_string(m_sleep_after_min) + " min";
    p.draw_text(txui::Point(cx + cw - 70.0, y1 + 74.0), sl_str, TXT_PRI, 12.5, true);
    if (m_sleep_slider) {
        m_sleep_slider->paint(p);
    }

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 112.0, cw - 40.0, 1.0), DIVIDER);

    // Row 3: Energy Mode
    p.draw_text(txui::Point(cx + 20.0, y1 + 130.0), "Energy Mode", TXT_PRI, 13.5, true);
    if (m_power_profile_control) {
        m_power_profile_control->paint(p);
    }

    // Card 2: Clipboard Subsystem
    float64 y2 = y1 + 192.0;
    draw_card(p, txui::Rect(cx, y2, cw, 136.0));
    p.draw_text(txui::Point(cx + 20.0, y2 + 20.0), "Clipboard Subsystem", TXT_PRI, 14.5, true);
    float64 clip_w = txui::FontMetrics::measure("Clipboard Subsystem", 14.5).width;
    draw_badge_pill(p, cx + 20.0 + clip_w + 12.0, y2 + 18.0, "Active (Live)", SUCCESS_BG, SUCCESS_TXT);
    p.draw_text(txui::Point(cx + 20.0, y2 + 48.0), "Live Wayland clipboard is active. History daemon (tinexus-clip) in Phase 2.", TXT_DIM, 12.0);

    p.fill_rect(txui::Rect(cx + 20.0, y2 + 78.0, cw - 40.0, 1.0), DIVIDER);

    p.draw_text(txui::Point(cx + 20.0, y2 + 98.0), "History Buffer Size:", TXT_SEC, 12.5);
    if (m_clipboard_size_control) {
        m_clipboard_size_control->paint(p);
    }

    // Card 3: Session Actions
    float64 y3 = y2 + 156.0;
    draw_card(p, txui::Rect(cx, y3, cw, 96.0));
    p.draw_text(txui::Point(cx + 20.0, y3 + 18.0), "Session Actions", TXT_PRI, 14.5, true);

    if (m_session_lock_btn)     m_session_lock_btn->paint(p);
    if (m_session_suspend_btn)  m_session_suspend_btn->paint(p);
    if (m_session_reboot_btn)   m_session_reboot_btn->paint(p);
    if (m_session_shutdown_btn) m_session_shutdown_btn->paint(p);

    if (!m_session_status_msg.empty()) {
        const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
        const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);
        p.draw_text(txui::Point(cx + 20.0, y3 + 86.0), m_session_status_msg, active_accent, 11.5);
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
    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "Global System Bindings", TXT_PRI, 15.0, true);

    for (int i = 0; i < 9; ++i) {
        float64 sy = y1 + 54.0 + static_cast<double>(i) * 28.0;
        p.draw_text(txui::Point(cx + 20.0, sy + 5.0), shortcuts[i].action, TXT_SEC, 12.5);

        float64 kx = cx + cw - 150.0;
        if (shortcuts[i].mod[0] != '\0') {
            draw_keycap(p, kx, sy, shortcuts[i].mod);
            p.draw_text(txui::Point(kx + 50.0, sy + 5.0), "+", TXT_DIM, 12.0, true);
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
    draw_page_header(p, area, "Privacy & Security",
                     "Application Gatekeeper, cryptographic integrity, and authentication",
                     SettingsPage::PrivacySecurity);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 94.0;

    // Card 1: Authentication Options
    draw_card(p, txui::Rect(cx, y1, cw, 158.0));

    p.draw_text(txui::Point(cx + 20.0, y1 + 20.0), "Require password when waking from sleep", TXT_PRI, 13.5, true);
    p.draw_text(txui::Point(cx + 20.0, y1 + 48.0), "Invokes tinexus-lock on system resume", TXT_DIM, 11.5);
    if (m_lock_sleep_toggle) {
        m_lock_sleep_toggle->paint(p);
    }

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 78.0, cw - 40.0, 1.0), DIVIDER);

    p.draw_text(txui::Point(cx + 20.0, y1 + 98.0), "Enable PAM Biometric Authentication", TXT_PRI, 13.5, true);
    float64 pam_label_w = txui::FontMetrics::measure("Enable PAM Biometric Authentication", 13.5).width;
    draw_badge_pill(p, cx + 20.0 + pam_label_w + 10.0, y1 + 96.0, "Biometrics Only - Password Mandatory", SUCCESS_BG, SUCCESS_TXT, 10.0);
    p.draw_text(txui::Point(cx + 20.0, y1 + 126.0), "Fingerprint and smartcard login via PAM modules (Password authentication always enforced)", TXT_DIM, 11.5);
    if (m_pam_auth_toggle) {
        m_pam_auth_toggle->paint(p);
    }

    // Card 2: Application Gatekeeper
    float64 y2 = y1 + 178.0;
    draw_card(p, txui::Rect(cx, y2, cw, 158.0));

    p.draw_text(txui::Point(cx + 20.0, y2 + 20.0), "Application Gatekeeper", TXT_PRI, 15.0, true);
    p.draw_text(txui::Point(cx + 20.0, y2 + 48.0), "Scans /opt/tinexus-apps for cryptographically verified application bundles", TXT_DIM, 11.5);
    if (m_rescan_btn) {
        m_rescan_btn->paint(p);
    }

    p.fill_rect(txui::Rect(cx + 20.0, y2 + 78.0, cw - 40.0, 1.0), DIVIDER);

    if (m_unverified_apps.empty()) {
        draw_badge_pill(p, cx + 20.0, y2 + 104.0, "All Installed Applications Verified", SUCCESS_BG, SUCCESS_TXT, 11.5);
        p.draw_text(txui::Point(cx + 280.0, y2 + 106.0), "Zero cryptographic integrity violations detected", TXT_DIM, 11.5);
    } else {
        float64 uy = y2 + 96.0;
        for (auto& app : m_unverified_apps) {
            p.draw_text(txui::Point(cx + 20.0, uy + 5.0), app.name, DANGER_TXT, 12.0, true);
            app.btn_rect = txui::Rect(cx + cw - 100.0, uy, 80.0, 24.0);
            p.fill_rounded_rect(app.btn_rect, 5.0, txui::Color(80, 25, 25, 220));
            p.draw_text(txui::Point(app.btn_rect.x() + 18.0, app.btn_rect.y() + 4.0), "Trust", TXT_PRI, 11.0, true);
            uy += 32.0;
        }
    }
}

// ── 7. Page: About ───────────────────────────────────────────────────────────
void SettingsWidget::paint_about_page(txui::Painter& p, const txui::Rect& area) const noexcept {
    draw_page_header(p, area, "About Tinexus",
                     "Operating system specifications, hardware detection, and platform versions",
                     SettingsPage::About);

    float64 cx = area.x();
    float64 cw = area.width();
    float64 y1 = area.y() + 94.0;

    // Card 1: System Specs
    draw_card(p, txui::Rect(cx, y1, cw, 240.0));

    // Logo tile
    draw_icon_badge_base(p, cx + 24.0, y1 + 24.0, txui::Color(0, 195, 255, 255), txui::Color(0, 120, 220, 255), 48.0, 12.0);
    p.draw_text(txui::Point(cx + 38.0, y1 + 34.0), "T", txui::Color(255, 255, 255, 255), 24.0, true);

    p.draw_text(txui::Point(cx + 90.0, y1 + 24.0), "Tinexus Desktop Platform", TXT_PRI, 18.0, true);
    p.draw_text(txui::Point(cx + 90.0, y1 + 52.0), "Architecture Freeze v1.1 Complete  •  Wayland-native Linux Shell", TXT_SEC, 12.5);

    p.fill_rect(txui::Rect(cx + 20.0, y1 + 90.0, cw - 40.0, 1.0), DIVIDER);

    struct SpecRow { const char* label; const std::string& val; };
    SpecRow specs[] = {
        {"OS Version:",    m_os_version},
        {"Processor:",     m_cpu_model},
        {"System Memory:", m_mem_info},
        {"Compositor:",    m_comp_info}
    };

    for (int i = 0; i < 4; ++i) {
        float64 sy = y1 + 104.0 + static_cast<double>(i) * 32.0;
        p.draw_text(txui::Point(cx + 20.0, sy), specs[i].label, TXT_SEC, 12.5);
        p.draw_text(txui::Point(cx + 160.0, sy), specs[i].val, TXT_PRI, 12.5, true);
    }
}

// ── Vector Icon Drawing Methods ──────────────────────────────────────────────
void SettingsWidget::draw_icon_display(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(55, 130, 245, 255), txui::Color(28, 92, 210, 255));
    float64 cx = tx + 10.0, cy = ty + 10.0;
    p.fill_rect(txui::Rect(cx - 6.0, cy - 5.0, 12.0, 8.0), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 4.5, cy - 3.5, 9.0, 5.0), txui::Color(35, 100, 220, 255));
    p.fill_rect(txui::Rect(cx - 1.0, cy + 3.0, 2.0, 2.0), TXT_PRI);
    p.fill_rect(txui::Rect(cx - 3.0, cy + 5.0, 6.0, 1.0), TXT_PRI);
}

void SettingsWidget::draw_icon_sound(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(245, 158, 11, 255), txui::Color(217, 119, 6, 255));
    float64 cx = tx + 10.0, cy = ty + 10.0;
    p.fill_rect(txui::Rect(cx - 5.0, cy - 2.5, 3.5, 5.0), TXT_PRI);
    p.draw_line(txui::Point(cx - 1.5, cy - 2.5), txui::Point(cx + 2.0, cy - 5.0), 1.5, TXT_PRI);
    p.draw_line(txui::Point(cx + 2.0, cy - 5.0), txui::Point(cx + 2.0, cy + 5.0), 1.5, TXT_PRI);
    p.draw_line(txui::Point(cx + 2.0, cy + 5.0), txui::Point(cx - 1.5, cy + 2.5), 1.5, TXT_PRI);
    p.draw_line(txui::Point(cx + 4.0, cy - 3.0), txui::Point(cx + 5.5, cy), 1.2, TXT_PRI);
    p.draw_line(txui::Point(cx + 5.5, cy), txui::Point(cx + 4.0, cy + 3.0), 1.2, TXT_PRI);
}

void SettingsWidget::draw_icon_personalization(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(255, 45, 85, 255), txui::Color(200, 20, 60, 255));
    p.fill_circle(txui::Point(tx + 7.0, ty + 7.0), 2.5, txui::Color(255, 255, 255, 255));
    p.fill_circle(txui::Point(tx + 13.0, ty + 7.0), 2.5, txui::Color(255, 255, 255, 255));
    p.fill_circle(txui::Point(tx + 7.0, ty + 13.0), 2.5, txui::Color(255, 255, 255, 255));
    p.fill_circle(txui::Point(tx + 13.0, ty + 13.0), 2.5, txui::Color(255, 255, 255, 255));
}

void SettingsWidget::draw_icon_network(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(0, 195, 255, 255), txui::Color(0, 140, 210, 255));
    draw_wifi_signal_bars(p, tx + 4.0, ty + 4.0, 4, txui::Color(255, 255, 255, 255));
}

void SettingsWidget::draw_icon_system(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(255, 149, 0, 255), txui::Color(210, 100, 0, 255));
    p.draw_circle(txui::Point(tx + 10.0, ty + 10.0), 4.5, 1.5, txui::Color(255, 255, 255, 255));
    p.fill_circle(txui::Point(tx + 10.0, ty + 10.0), 2.0, txui::Color(255, 255, 255, 255));
}

void SettingsWidget::draw_icon_keyboard(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(110, 135, 250, 255), txui::Color(75, 100, 215, 255));
    float64 cx = tx + 10.0, cy = ty + 10.0;
    p.fill_rounded_rect(txui::Rect(cx - 6.0, cy - 4.0, 12.0, 8.0), 1.5, TXT_PRI);
    p.fill_rounded_rect(txui::Rect(cx - 4.5, cy - 2.5, 9.0, 5.0), 1.0, txui::Color(85, 110, 230, 255));
    p.fill_rect(txui::Rect(cx - 2.5, cy - 0.5, 5.0, 1.5), TXT_PRI);
}

void SettingsWidget::draw_icon_privacy(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(52, 199, 89, 255), txui::Color(30, 150, 60, 255));
    draw_lock_icon(p, tx + 6.0, ty + 4.0, txui::Color(255, 255, 255, 255));
}

void SettingsWidget::draw_icon_about(txui::Painter& p, float64 tx, float64 ty) const noexcept {
    draw_icon_badge_base(p, tx, ty, txui::Color(88, 86, 214, 255), txui::Color(60, 50, 170, 255));
    p.draw_text(txui::Point(tx + 7.5, ty + 3.5), "i", txui::Color(255, 255, 255, 255), 13.0, true);
}

void SettingsWidget::draw_wifi_signal_bars(txui::Painter& p, float64 x, float64 y, int bars, const txui::Color& active_col) const noexcept {
    const float64 bar_w = 2.5;
    const float64 bar_gap = 1.5;
    const float64 heights[] = {4.0, 7.0, 10.0, 13.0};

    for (int i = 0; i < 4; ++i) {
        float64 bx = x + static_cast<double>(i) * (bar_w + bar_gap);
        float64 by = y + (13.0 - heights[i]);
        txui::Color col = (i < bars) ? active_col : txui::Color(60, 60, 75, 180);
        p.fill_rounded_rect(txui::Rect(bx, by, bar_w, heights[i]), 1.0, col);
    }
}

void SettingsWidget::draw_lock_icon(txui::Painter& p, float64 x, float64 y, const txui::Color& col) const noexcept {
    p.draw_circle(txui::Point(x + 4.0, y + 4.0), 3.0, 1.4, col);
    p.fill_rounded_rect(txui::Rect(x, y + 5.0, 8.0, 7.0), 1.8, col);
}

} // namespace tinexus::settings_ui
