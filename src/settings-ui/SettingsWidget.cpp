// ─────────────────────────────────────────────────────────────────────────────
// SettingsWidget.cpp  — Tinexus Control Center
// macOS/Elementary-inspired premium dark settings UI with live configuration
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/reboot.h>
#include <linux/reboot.h>

namespace {

using txui::float64;
using txui::uint8;

// ── Design Token Palette (docs/05_UI_UX_GUIDELINES.md) ──────────────────────
constexpr txui::Color BG_BASE      { 8,  8, 12, 255};
constexpr txui::Color BG_SURFACE   {16, 16, 24, 255};
constexpr txui::Color BG_ELEVATED  {24, 24, 36, 255};
constexpr txui::Color BG_SIDEBAR   {12, 12, 20, 255};

constexpr txui::Color CARD_TOP     {22, 22, 34, 230};
constexpr txui::Color CARD_BOT     {18, 18, 28, 200};

constexpr txui::Color TXT_PRI      {240, 240, 248, 255};
constexpr txui::Color TXT_SEC      {140, 140, 170, 220};
constexpr txui::Color TXT_DIM      { 80,  80, 100, 180};

constexpr txui::Color SUCCESS      { 80, 200, 130, 255};
constexpr txui::Color WARNING      {230, 160,  50, 255};
constexpr txui::Color DANGER       {239,  68,  68, 255};
constexpr txui::Color DIVIDER      { 36,  36,  52, 255};

// Accent Palette Options
const tinexus::settings_ui::AccentOption ACCENT_PALETTE[] = {
    {107, 140, 239, "Horizon Blue"},
    { 80, 200, 130, "Emerald Green"},
    {230, 130,  60, "Solar Orange"},
    {200,  80, 170, "Cosmic Violet"},
    {220, 190,  50, "Amber Gold"},
    { 80, 190, 220, "Aqua Cyan"}
};
constexpr int ACCENT_COUNT = 6;

static inline txui::Color darken(const txui::Color& c, double factor = 0.8, uint8 alpha = 255) {
    return txui::Color(
        static_cast<uint8>(static_cast<double>(c.r()) * factor),
        static_cast<uint8>(static_cast<double>(c.g()) * factor),
        static_cast<uint8>(static_cast<double>(c.b()) * factor),
        alpha
    );
}

static inline txui::Color darken(uint8 r, uint8 g, uint8 b, double factor = 0.8, uint8 alpha = 255) {
    return txui::Color(
        static_cast<uint8>(static_cast<double>(r) * factor),
        static_cast<uint8>(static_cast<double>(g) * factor),
        static_cast<uint8>(static_cast<double>(b) * factor),
        alpha
    );
}

static void draw_card(txui::Painter& painter, const txui::Rect& r, const std::string& title) {
    painter.fill_rounded_rect(
        txui::Rect(r.x() - 2, r.y() + 4, r.width() + 4, r.height() + 4),
        16.0, txui::Color(0, 0, 0, 180)
    );
    painter.fill_gradient_rounded_rect(r, 14.0, CARD_TOP, CARD_BOT);
    painter.fill_rounded_rect(
        txui::Rect(r.x() + 1, r.y() + 1, r.width() - 2, 1),
        0.5, txui::Color(255, 255, 255, 30)
    );
    painter.draw_text(txui::Point(r.x() + 20, r.y() + 16), title, TXT_PRI, 1.0);
    painter.fill_gradient_rect(
        txui::Rect(r.x() + 16, r.y() + 34, r.width() - 32, 1),
        txui::Color(100, 100, 160, 60),
        txui::Color(100, 100, 160, 0),
        true
    );
}

static void draw_row(txui::Painter& painter, const txui::Rect& card, float64 row_y,
                     const std::string& label, const std::string& value,
                     txui::Color value_color = TXT_SEC) {
    painter.draw_text(txui::Point(card.x() + 20, card.y() + row_y), label, TXT_SEC, 1.0);
    const float64 val_x = card.x() + card.width() - static_cast<float64>(value.size()) * 8.0 - 20.0;
    painter.draw_text(txui::Point(val_x, card.y() + row_y), value, value_color, 1.0);
}

static void draw_toggle(txui::Painter& painter, float64 tx, float64 ty, bool on, const txui::Color& accent_col) {
    painter.fill_gradient_rounded_rect(
        txui::Rect(tx, ty, 42, 22), 11.0,
        on ? accent_col : txui::Color(40, 40, 60, 220),
        on ? darken(accent_col, 0.8, 180) : txui::Color(30, 30, 50, 200)
    );
    const float64 knob_x = on ? tx + 22 : tx + 2;
    painter.fill_circle(txui::Point(knob_x + 9, ty + 11), 9.0,
                        txui::Color(240, 240, 255, 240));
    if (on) {
        painter.draw_glow(txui::Point(knob_x + 9, ty + 11), 9.0, 18.0, accent_col);
    }
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

    // 3. Read Memory
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

    scan_wallpapers();
    load_config();
    refresh_unverified_apps();
}

void SettingsWidget::scan_wallpapers() {
    m_wallpapers.clear();

    // Curated Wallpapers
    m_wallpapers.push_back({"Default Horizon", "/usr/share/backgrounds/tinexus-default.jpg", 15, 23, 42});
    m_wallpapers.push_back({"Midnight Geometry", "/usr/share/backgrounds/midnight.jpg", 12, 16, 28});
    m_wallpapers.push_back({"Aurora Waves", "/usr/share/backgrounds/aurora.jpg", 20, 45, 65});
    m_wallpapers.push_back({"Deep Space", "/usr/share/backgrounds/space.jpg", 8, 8, 18});

    // Check disk directory for additional user wallpapers
    std::string bg_dir = "/usr/share/backgrounds";
    if (std::filesystem::exists(bg_dir)) {
        for (const auto& entry : std::filesystem::directory_iterator(bg_dir)) {
            if (!entry.is_regular_file()) continue;
            std::string path = entry.path().string();
            std::string fname = entry.path().filename().string();
            if (fname.ends_with(".jpg") || fname.ends_with(".png")) {
                bool already_in = false;
                for (const auto& w : m_wallpapers) {
                    if (w.path == path) { already_in = true; break; }
                }
                if (!already_in) {
                    m_wallpapers.push_back({fname, path, 25, 35, 55});
                }
            }
        }
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
            if (eq != std::string::npos) m_selected_wallpaper_idx = std::clamp(std::stoi(line.substr(eq + 1)), 0, static_cast<int>(m_wallpapers.size()) - 1);
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
        } else if (line.find("power_profile") != std::string::npos) {
            auto start = line.find('"');
            auto end = line.find('"', start + 1);
            if (start != std::string::npos && end != std::string::npos) m_power_profile = line.substr(start + 1, end - start - 1);
        } else if (line.find("lock_on_sleep") != std::string::npos) {
            m_lock_on_sleep = (line.find("true") != std::string::npos);
        } else if (line.find("pam_auth") != std::string::npos) {
            m_pam_auth = (line.find("true") != std::string::npos);
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
    out << "power_profile = \"" << m_power_profile << "\"\n\n";

    out << "[security]\n";
    out << "lock_on_sleep = " << (m_lock_on_sleep ? "true" : "false") << "\n";
    out << "pam_auth = " << (m_pam_auth ? "true" : "false") << "\n";

    out.flush();
    out.close();

    std::filesystem::rename(temp_path, config_path);
}

void SettingsWidget::trigger_session_action(int action_idx) {
    if (action_idx == 0) {
        m_session_status_msg = "Launching Lock Screen...";
        if (fork() == 0) {
            execlp("tinexus-lock", "tinexus-lock", nullptr);
            _exit(0);
        }
    } else if (action_idx == 1) {
        m_session_status_msg = "Entering Sleep Mode...";
        if (fork() == 0) {
            execlp("tinexus-lock", "tinexus-lock", nullptr);
            _exit(0);
        }
    } else if (action_idx == 2) {
        m_session_status_msg = "Rebooting System...";
        sync();
        reboot(RB_AUTOBOOT);
    } else if (action_idx == 3) {
        m_session_status_msg = "Shutting Down System...";
        sync();
        reboot(RB_POWER_OFF);
    }
}

void SettingsWidget::select_page(SettingsPage page) {
    m_current_page = page;
    mark_needs_paint();
}

txui::Size SettingsWidget::measure_override(const txui::Constraints& c) noexcept {
    return c.constrain(txui::Size(c.max_width, c.max_height));
}

void SettingsWidget::layout_override(const txui::Rect& f) noexcept {
    constexpr double CONTENT_PADDING = 20.0;
    m_sidebar_rect = txui::Rect(f.x(), f.y(), SIDEBAR_W, f.height());
    m_content_rect = txui::Rect(
        f.x() + SIDEBAR_W + CONTENT_PADDING,
        f.y() + CONTENT_PADDING,
        f.width() - SIDEBAR_W - CONTENT_PADDING * 2.0,
        f.height() - CONTENT_PADDING * 2.0
    );
}

bool SettingsWidget::handle_event(const txui::Event& event) noexcept {
    if (event.type == txui::EventType::PointerMove) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        int new_hover = -1;
        if (px >= m_sidebar_rect.x() && px <= m_sidebar_rect.right()) {
            if (py >= ITEM_Y0 && py <= ITEM_Y0 + ITEM_H) new_hover = 0;
            else if (py >= ITEM_Y0 + ITEM_H + 4 && py <= ITEM_Y0 + (ITEM_H + 4) * 2 - 4) new_hover = 1;
            else if (py >= ITEM_Y0 + (ITEM_H + 4) * 2 && py <= ITEM_Y0 + (ITEM_H + 4) * 3 - 4) new_hover = 2;
            else if (py >= ITEM_Y0 + (ITEM_H + 4) * 3 && py <= ITEM_Y0 + (ITEM_H + 4) * 4 - 4) new_hover = 3;
            else if (py >= ITEM_Y0 + (ITEM_H + 4) * 4 && py <= ITEM_Y0 + (ITEM_H + 4) * 5 - 4) new_hover = 4;
        }

        if (new_hover != m_hovered_tab) {
            m_hovered_tab = new_hover;
            mark_needs_paint();
        }

        if (m_current_page == SettingsPage::PrivacySecurity) {
            bool needs_paint = false;
            for (auto& app : m_unverified_apps) {
                bool hover = (px >= app.btn_rect.x() && px <= app.btn_rect.right() &&
                              py >= app.btn_rect.y() && py <= app.btn_rect.bottom());
                if (app.is_hovered != hover) {
                    app.is_hovered = hover;
                    needs_paint = true;
                }
            }
            if (needs_paint) mark_needs_paint();
        }
        return false;
    } else if (event.type == txui::EventType::PointerButtonPress && event.pointer.button == txui::MouseButton::Left) {
        double px = event.pointer.x;
        double py = event.pointer.y;

        // 1. Sidebar tab navigation
        if (px >= m_sidebar_rect.x() && px <= m_sidebar_rect.right()) {
            if (m_hovered_tab == 0) { select_page(SettingsPage::Display); return true; }
            else if (m_hovered_tab == 1) { select_page(SettingsPage::Personalization); return true; }
            else if (m_hovered_tab == 2) { select_page(SettingsPage::System); return true; }
            else if (m_hovered_tab == 3) { select_page(SettingsPage::PrivacySecurity); return true; }
            else if (m_hovered_tab == 4) { select_page(SettingsPage::About); return true; }
        }

        const float64 x = m_content_rect.x() + 40;
        const float64 cw = m_content_rect.width() - 80;

        // 2. Interactive Display Page Controls
        if (m_current_page == SettingsPage::Display) {
            float64 y = m_content_rect.y() + 84;
            y += 130; // after Monitor card

            // Scale & Layout card
            txui::Rect c2(x, y, cw, 88);
            if (py >= c2.y() + 40 && py <= c2.y() + 74) {
                for (int i = 0; i < 4; ++i) {
                    float64 pill_x = c2.x() + cw - 240.0 + static_cast<double>(i) * 55.0;
                    if (px >= pill_x && px <= pill_x + 50.0) {
                        m_display_scale_idx = i;
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }
            y += 98;

            // Night Light Toggle
            txui::Rect c3(x, y, cw, 72);
            if (px >= x + cw - 70 && px <= x + cw - 10 && py >= c3.y() + 30 && py <= c3.y() + 66) {
                m_night_light_enabled = !m_night_light_enabled;
                save_config();
                mark_needs_paint();
                return true;
            }
            y += 82;

            // VRR Toggle
            txui::Rect c4(x, y, cw, 68);
            if (px >= x + cw - 70 && px <= x + cw - 10 && py >= c4.y() + 30 && py <= c4.y() + 66) {
                m_vrr_enabled = !m_vrr_enabled;
                save_config();
                mark_needs_paint();
                return true;
            }
        }

        // 3. Interactive Personalization Page Controls
        if (m_current_page == SettingsPage::Personalization) {
            float64 y = m_content_rect.y() + 84;

            // Accent Color Card
            txui::Rect c1(x, y, cw, 100);
            if (py >= c1.y() + 45 && py <= c1.y() + 85) {
                for (int i = 0; i < ACCENT_COUNT; ++i) {
                    float64 cx_ = x + 22 + static_cast<double>(i) * 50.0;
                    if (px >= cx_ - 20.0 && px <= cx_ + 20.0) {
                        m_selected_accent_idx = i;
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }
            y += 110;

            // Appearance Theme Card
            txui::Rect c2(x, y, cw, 80);
            if (py >= c2.y() + 40 && py <= c2.y() + 76) {
                constexpr float64 pill_w = 96;
                if (px >= x + 20 && px <= x + 20 + pill_w) {
                    m_theme_mode = "Dark";
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (px >= x + 20 + pill_w + 8 && px <= x + 20 + (pill_w * 2) + 8) {
                    m_theme_mode = "Light";
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
            y += 90;

            // Wallpaper Cards
            txui::Rect c3(x, y, cw, 140);
            if (py >= c3.y() + 45 && py <= c3.y() + 125) {
                const size_t num_wallpapers = std::min(m_wallpapers.size(), size_t{4});
                const double card_w = (cw - 40.0 - static_cast<double>(num_wallpapers - 1) * 14.0) / static_cast<double>(num_wallpapers);
                for (size_t i = 0; i < num_wallpapers; ++i) {
                    double wx = x + 20.0 + static_cast<double>(i) * (card_w + 14.0);
                    if (px >= wx && px <= wx + card_w) {
                        m_selected_wallpaper_idx = static_cast<int>(i);
                        save_config();
                        mark_needs_paint();
                        return true;
                    }
                }
            }
        }

        // 4. Interactive System Page Controls
        if (m_current_page == SettingsPage::System) {
            float64 y = m_content_rect.y() + 84;

            // Power Settings Card
            txui::Rect c1(x, y, cw, 112);
            if (py >= c1.y() + 46 && py <= c1.y() + 68) {
                if (px >= c1.x() + cw - 120 && px <= c1.x() + cw - 90) {
                    m_screen_timeout_min = std::max(1, m_screen_timeout_min - 1);
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (px >= c1.x() + cw - 40 && px <= c1.x() + cw - 10) {
                    m_screen_timeout_min = std::min(60, m_screen_timeout_min + 5);
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
            if (py >= c1.y() + 68 && py <= c1.y() + 90) {
                if (px >= c1.x() + cw - 120 && px <= c1.x() + cw - 90) {
                    m_sleep_after_min = std::max(5, m_sleep_after_min - 5);
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (px >= c1.x() + cw - 40 && px <= c1.x() + cw - 10) {
                    m_sleep_after_min = std::min(120, m_sleep_after_min + 5);
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
            y += 122;

            // Security & Privacy Toggles
            txui::Rect c2(x, y, cw, 112);
            if (px >= x + cw - 70 && px <= x + cw - 10) {
                if (py >= c2.y() + 30 && py <= c2.y() + 60) {
                    m_lock_on_sleep = !m_lock_on_sleep;
                    save_config();
                    mark_needs_paint();
                    return true;
                } else if (py >= c2.y() + 66 && py <= c2.y() + 96) {
                    m_pam_auth = !m_pam_auth;
                    save_config();
                    mark_needs_paint();
                    return true;
                }
            }
            y += 122;

            // Session Action Buttons: Lock, Sleep, Restart, Shut Down
            txui::Rect c3(x, y, cw, 80);
            if (py >= c3.y() + 44 && py <= c3.y() + 76) {
                for (int i = 0; i < 4; ++i) {
                    const float64 bx = x + 20 + static_cast<double>(i) * 118.0;
                    if (px >= bx && px <= bx + 106.0) {
                        trigger_session_action(i);
                        mark_needs_paint();
                        return true;
                    }
                }
            }
        }

        // 5. Privacy & Security Page Controls
        if (m_current_page == SettingsPage::PrivacySecurity) {
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

// ─────────────────────────────────────────────────────────────────────────────
// Paint Root
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    // 1. Full window background
    painter.fill_gradient_rect(frame(),
        txui::Color( 6,  6, 14, 255),
        txui::Color(10, 10, 18, 255));

    // 2. Sidebar atmosphere glow
    painter.fill_circle(
        txui::Point(SIDEBAR_W * 0.4, frame().height() * 0.15),
        SIDEBAR_W * 0.9,
        txui::Color(current_accent.r, current_accent.g, current_accent.b, 20)
    );

    // 3. Sidebar background
    painter.fill_gradient_rect(m_sidebar_rect,
        txui::Color(14, 14, 22, 255),
        txui::Color(10, 10, 18, 255));

    // 4. Sidebar Title + Accent Logo Dot
    painter.fill_circle(
        txui::Point(m_sidebar_rect.x() + 20, m_sidebar_rect.y() + 30),
        6.0, active_accent
    );
    painter.draw_glow(
        txui::Point(m_sidebar_rect.x() + 20, m_sidebar_rect.y() + 30),
        6.0, 14.0, active_accent
    );
    painter.draw_text(
        txui::Point(m_sidebar_rect.x() + 34, m_sidebar_rect.y() + 22),
        "Settings", TXT_PRI, 2.0
    );

    // 5. Sidebar Navigation Items
    paint_sidebar(painter);

    // 6. Vertical separator line
    for (int i = 0; i < static_cast<int>(frame().height()); ++i) {
        const double t = static_cast<double>(i) / frame().height();
        const uint8 alpha = static_cast<uint8>(50.0 * (1.0 - t * t));
        painter.fill_rect(
            txui::Rect(m_sidebar_rect.right() - 1, frame().y() + i, 1, 1),
            txui::Color(current_accent.r, current_accent.g, current_accent.b, alpha)
        );
    }

    // 7. Content background
    painter.fill_gradient_rect(m_content_rect,
        txui::Color( 8,  8, 14, 255),
        txui::Color( 6,  6, 12, 255));

    // 8. Dispatch current page
    switch (m_current_page) {
        case SettingsPage::Display:         paint_display_page(painter, m_content_rect);         break;
        case SettingsPage::Personalization: paint_personalization_page(painter, m_content_rect); break;
        case SettingsPage::System:          paint_system_page(painter, m_content_rect);          break;
        case SettingsPage::PrivacySecurity: paint_privacy_security_page(painter, m_content_rect);break;
        case SettingsPage::About:           paint_about_page(painter, m_content_rect);           break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_sidebar(txui::Painter& painter) const noexcept {
    const float64 sy = m_sidebar_rect.y();
    const float64 search_y = sy + 76.0;
    const float64 search_x = m_sidebar_rect.x() + 10;
    const float64 search_w = SIDEBAR_W - 20;

    // Search bar container
    painter.fill_rounded_rect(txui::Rect(search_x, search_y, search_w, 32.0), 8.0, txui::Color(255, 255, 255, 12));
    painter.fill_rounded_rect(txui::Rect(search_x + 1, search_y + 1, search_w - 2, 30.0), 7.0, txui::Color(0, 0, 0, 80));

    // Search magnifying icon
    const float64 icon_cx = search_x + 16.0;
    const float64 icon_cy = search_y + 16.0;
    painter.draw_circle(txui::Point(icon_cx, icon_cy), 5.0, 1.5, TXT_SEC);
    painter.fill_rect(txui::Rect(icon_cx + 3.0, icon_cy + 3.0, 4.0, 1.5), TXT_SEC);

    painter.draw_text(txui::Point(search_x + 32, search_y + 8), "Quick Search", TXT_SEC, 1.0);

    // Sidebar items
    const float64 items_start_y = search_y + 44.0;
    paint_sidebar_item(painter, "Display",         SettingsPage::Display,         items_start_y);
    paint_sidebar_item(painter, "Personalization", SettingsPage::Personalization, items_start_y + ITEM_H + 4);
    paint_sidebar_item(painter, "System",          SettingsPage::System,          items_start_y + (ITEM_H + 4) * 2);
    paint_sidebar_item(painter, "Privacy",         SettingsPage::PrivacySecurity, items_start_y + (ITEM_H + 4) * 3);
    paint_sidebar_item(painter, "About",           SettingsPage::About,           items_start_y + (ITEM_H + 4) * 4);
}

void SettingsWidget::paint_sidebar_item(txui::Painter& painter, const std::string& label,
                                        SettingsPage page, float64 y) const noexcept {
    const bool active = (m_current_page == page);
    const float64 x   = m_sidebar_rect.x() + 10;
    const float64 iw  = SIDEBAR_W - 20;
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    if (active) {
        painter.fill_gradient_rounded_rect(
            txui::Rect(x, y + 2, iw, ITEM_H - 4),
            10.0,
            txui::Color(current_accent.r, current_accent.g, current_accent.b, 80),
            txui::Color(current_accent.r, current_accent.g, current_accent.b, 30)
        );
        painter.fill_gradient_rounded_rect(
            txui::Rect(x + 2, y + 8, 3, ITEM_H - 16),
            1.5,
            active_accent,
            txui::Color(current_accent.r, current_accent.g, current_accent.b, 140)
        );
    } else {
        int tab_idx = -1;
        if (page == SettingsPage::Display) tab_idx = 0;
        else if (page == SettingsPage::Personalization) tab_idx = 1;
        else if (page == SettingsPage::System) tab_idx = 2;
        else if (page == SettingsPage::PrivacySecurity) tab_idx = 3;
        else if (page == SettingsPage::About) tab_idx = 4;

        if (m_hovered_tab == tab_idx) {
            painter.fill_rounded_rect(
                txui::Rect(x, y + 2, iw, ITEM_H - 4),
                10.0,
                txui::Color(255, 255, 255, 10)
            );
        }
    }

    // Icon Tile
    const float64 tile_x = x + 10;
    const float64 tile_y = y + ITEM_H * 0.5 - 14.0;
    txui::Rect tile_rect(tile_x, tile_y, 28, 28);

    if (page == SettingsPage::Display) {
        painter.fill_gradient_rounded_rect(tile_rect, 6.0, txui::Color(80, 150, 255, 255), txui::Color(40, 100, 240, 255));
        draw_icon_display(painter, tile_x, tile_y);
    } else if (page == SettingsPage::Personalization) {
        painter.fill_gradient_rounded_rect(tile_rect, 6.0, active_accent, darken(current_accent.r, current_accent.g, current_accent.b, 0.8));
        draw_icon_personalization(painter, tile_x, tile_y);
    } else if (page == SettingsPage::System) {
        painter.fill_gradient_rounded_rect(tile_rect, 6.0, txui::Color(140, 140, 150, 255), txui::Color(100, 100, 110, 255));
        draw_icon_system(painter, tile_x, tile_y);
    } else if (page == SettingsPage::PrivacySecurity) {
        painter.fill_gradient_rounded_rect(tile_rect, 6.0, txui::Color(255, 180, 50, 255), txui::Color(240, 140, 30, 255));
        draw_icon_privacy(painter, tile_x, tile_y);
    } else if (page == SettingsPage::About) {
        painter.fill_gradient_rounded_rect(tile_rect, 6.0, txui::Color(50, 200, 180, 255), txui::Color(30, 160, 140, 255));
        draw_icon_about(painter, tile_x, tile_y);
    }

    const txui::Color col = active ? TXT_PRI : TXT_SEC;
    painter.draw_text(txui::Point(x + 48, y + ITEM_H * 0.5 - 7), label, col, 1.0);
}

void SettingsWidget::draw_icon_display(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    painter.fill_rounded_rect(txui::Rect(tx+5, ty+6, 18, 12), 2.0, txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+7, ty+8, 14, 8), txui::Color(80, 150, 255, 255));
    painter.fill_rect(txui::Rect(tx+12, ty+18, 4, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+8, ty+21, 12, 2.0), txui::Color(255, 255, 255, 255));
}

void SettingsWidget::draw_icon_personalization(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    painter.fill_circle(txui::Point(tx+14, ty+14), 8.0, txui::Color(255, 255, 255, 255));
    painter.fill_circle(txui::Point(tx+18, ty+15), 2.5, txui::Color(30, 30, 40, 255));
    painter.fill_circle(txui::Point(tx+10, ty+11), 1.5, txui::Color(240, 60, 90, 255));
    painter.fill_circle(txui::Point(tx+10, ty+17), 1.5, txui::Color(80, 200, 130, 255));
    painter.fill_circle(txui::Point(tx+14, ty+9), 1.5, txui::Color(107, 140, 239, 255));
}

void SettingsWidget::draw_icon_system(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    painter.fill_rounded_rect(txui::Rect(tx+7, ty+7, 14, 14), 2.0, txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+10, ty+10, 8, 8), txui::Color(110, 110, 120, 255));
    painter.fill_rect(txui::Rect(tx+9, ty+4, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+13, ty+4, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+17, ty+4, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+9, ty+21, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+13, ty+21, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+17, ty+21, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+4, ty+9, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+4, ty+13, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+4, ty+17, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+21, ty+9, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+21, ty+13, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+21, ty+17, 3, 2), txui::Color(255, 255, 255, 255));
}

void SettingsWidget::draw_icon_privacy(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    painter.fill_rect(txui::Rect(tx+9, ty+6, 2, 6), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+17, ty+6, 2, 6), txui::Color(255, 255, 255, 255));
    painter.fill_rounded_rect(txui::Rect(tx+9, ty+4, 10, 4), 1.5, txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+11, ty+6, 6, 6), txui::Color(240, 160, 40, 255));
    painter.fill_rounded_rect(txui::Rect(tx+7, ty+12, 14, 10), 2.0, txui::Color(255, 255, 255, 255));
    painter.fill_circle(txui::Point(tx+14, ty+16), 1.5, txui::Color(240, 160, 40, 255));
    painter.fill_rect(txui::Rect(tx+13, ty+17, 2, 3), txui::Color(240, 160, 40, 255));
}

void SettingsWidget::draw_icon_about(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    painter.fill_circle(txui::Point(tx+14, ty+14), 8.0, txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+13, ty+9, 2, 2), txui::Color(40, 180, 160, 255));
    painter.fill_rect(txui::Rect(tx+13, ty+12, 2, 5), txui::Color(40, 180, 160, 255));
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: Display
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_display_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    painter.draw_glow(txui::Point(x + 35, y + 10), 10, 50, active_accent);
    painter.draw_text(txui::Point(x, y), "Display", TXT_PRI, 2.0);
    y += 52;

    // Card 1: Monitor
    txui::Rect c1(x, y, cw, 120);
    draw_card(painter, c1, "Monitor");
    draw_row(painter, c1, 52, "Resolution",   "1920 x 1080 (Wayland Native)");
    draw_row(painter, c1, 76, "Refresh Rate", "60.00 Hz", SUCCESS);
    draw_row(painter, c1, 100,"Output Port",  "Virtual-1 / eDP-1");
    y += 130;

    // Card 2: Scale & Layout (Interactive Pills)
    txui::Rect c2(x, y, cw, 88);
    draw_card(painter, c2, "Scale & Layout");
    painter.draw_text(txui::Point(x + 20, c2.y() + 54), "Display Scale", TXT_SEC, 1.0);

    const char* scales[] = {"100%", "125%", "150%", "200%"};
    for (int i = 0; i < 4; ++i) {
        float64 pill_x = c2.x() + cw - 240.0 + static_cast<double>(i) * 55.0;
        bool is_sel = (m_display_scale_idx == i);
        painter.fill_gradient_rounded_rect(
            txui::Rect(pill_x, c2.y() + 44, 48, 24), 6.0,
            is_sel ? active_accent : txui::Color(35, 35, 50, 200),
            is_sel ? darken(current_accent.r, current_accent.g, current_accent.b, 0.8, 180) : txui::Color(25, 25, 38, 180)
        );
        painter.draw_text(txui::Point(pill_x + 8, c2.y() + 49), scales[i], is_sel ? TXT_PRI : TXT_SEC, 0.9, is_sel);
    }
    y += 98;

    // Card 3: Night Light
    txui::Rect c3(x, y, cw, 72);
    draw_card(painter, c3, "Night Light");
    painter.draw_text(txui::Point(x + 20, c3.y() + 52), "Reduce blue light after sunset for eye comfort", TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c3.y() + 38, m_night_light_enabled, active_accent);
    y += 82;

    // Card 4: VRR
    txui::Rect c4(x, y, cw, 68);
    draw_card(painter, c4, "Variable Refresh Rate (VRR)");
    painter.draw_text(txui::Point(x + 20, c4.y() + 50), "Adaptive sync / FreeSync support", TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c4.y() + 44, m_vrr_enabled, active_accent);
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: Personalization
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_personalization_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    painter.draw_glow(txui::Point(x + 80, y + 10), 10, 50, active_accent);
    painter.draw_text(txui::Point(x, y), "Personalization", TXT_PRI, 2.0);
    y += 52;

    // Card 1: Accent Colors (Interactive 6-color palette)
    txui::Rect c1(x, y, cw, 100);
    draw_card(painter, c1, "System Accent Color (" + current_accent.name + ")");
    for (int i = 0; i < ACCENT_COUNT; ++i) {
        const float64 cx_ = x + 22 + static_cast<double>(i) * 50.0;
        const float64 cy_ = c1.y() + 66;
        const auto& col = ACCENT_PALETTE[i];
        painter.fill_circle(txui::Point(cx_, cy_), 16.0,
            txui::Color(col.r, col.g, col.b, 230));

        if (i == m_selected_accent_idx) {
            painter.draw_circle(txui::Point(cx_, cy_), 19.0, 2.0,
                txui::Color(col.r, col.g, col.b, 255));
            painter.draw_glow(txui::Point(cx_, cy_), 16.0, 28.0,
                txui::Color(col.r, col.g, col.b, 200));
            painter.fill_circle(txui::Point(cx_, cy_), 4.0, txui::Color::white());
        }
    }
    y += 110;

    // Card 2: Theme (Dark / Light Pills)
    txui::Rect c2(x, y, cw, 80);
    draw_card(painter, c2, "Appearance Theme");
    constexpr float64 pill_w = 96, pill_h = 28;
    const float64 py = c2.y() + 44;
    bool is_dark = (m_theme_mode == "Dark");

    // Dark Pill
    painter.fill_gradient_rounded_rect(txui::Rect(x + 20, py, pill_w, pill_h), 8.0,
        is_dark ? active_accent : txui::Color(30, 30, 44, 200),
        is_dark ? darken(current_accent.r, current_accent.g, current_accent.b, 0.8, 200) : txui::Color(20, 20, 32, 200));
    painter.draw_text(txui::Point(x + 44, py + 7), "Dark", is_dark ? TXT_PRI : TXT_SEC, 1.0, is_dark);

    // Light Pill
    painter.fill_gradient_rounded_rect(txui::Rect(x + 20 + pill_w + 8, py, pill_w, pill_h), 8.0,
        !is_dark ? active_accent : txui::Color(30, 30, 44, 200),
        !is_dark ? darken(current_accent.r, current_accent.g, current_accent.b, 0.8, 200) : txui::Color(20, 20, 32, 200));
    painter.draw_text(txui::Point(x + 20 + pill_w + 30, py + 7), "Light", !is_dark ? TXT_PRI : TXT_SEC, 1.0, !is_dark);
    y += 90;

    // Card 3: Interactive Wallpaper Cards
    txui::Rect c3(x, y, cw, 140);
    draw_card(painter, c3, "Desktop Wallpaper");

    const size_t num_wallpapers = std::min(m_wallpapers.size(), size_t{4});
    const double card_w = (cw - 40.0 - static_cast<double>(num_wallpapers - 1) * 14.0) / static_cast<double>(num_wallpapers);

    for (size_t i = 0; i < num_wallpapers; ++i) {
        double wx = x + 20.0 + static_cast<double>(i) * (card_w + 14.0);
        double wy = c3.y() + 45.0;
        bool is_sel = (m_selected_wallpaper_idx == static_cast<int>(i));

        // Thumbnail simulation card
        painter.fill_gradient_rounded_rect(
            txui::Rect(wx, wy, card_w, 64.0), 8.0,
            txui::Color(static_cast<uint8>(m_wallpapers[i].preview_r + 20), static_cast<uint8>(m_wallpapers[i].preview_g + 20), static_cast<uint8>(m_wallpapers[i].preview_b + 30), 240),
            txui::Color(m_wallpapers[i].preview_r, m_wallpapers[i].preview_g, m_wallpapers[i].preview_b, 240)
        );

        if (is_sel) {
            painter.draw_circle(txui::Point(wx + card_w - 14, wy + 14), 8.0, 2.0, active_accent);
            painter.fill_circle(txui::Point(wx + card_w - 14, wy + 14), 4.0, active_accent);
        }

        painter.draw_text(txui::Point(wx + 4, wy + 70), m_wallpapers[i].name, is_sel ? active_accent : TXT_SEC, 0.85, is_sel);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: System
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_system_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    painter.draw_glow(txui::Point(x + 35, y + 10), 10, 50, active_accent);
    painter.draw_text(txui::Point(x, y), "System & Power", TXT_PRI, 2.0);
    y += 52;

    // Card 1: Power & Battery Management
    txui::Rect c1(x, y, cw, 112);
    draw_card(painter, c1, "Power & Sleep");
    draw_row(painter, c1, 52, "Screen Timeout",  "[-]  " + std::to_string(m_screen_timeout_min) + " min  [+]");
    draw_row(painter, c1, 74, "Sleep After",     "[-]  " + std::to_string(m_sleep_after_min) + " min  [+]");
    draw_row(painter, c1, 96, "Power Profile",   m_power_profile, SUCCESS);
    y += 122;

    // Card 2: Security & Authentication
    txui::Rect c2(x, y, cw, 112);
    draw_card(painter, c2, "Security & Lock Screen");
    painter.draw_text(txui::Point(x + 20, c2.y() + 52), "Require password when waking from sleep", TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c2.y() + 38, m_lock_on_sleep, active_accent);
    painter.draw_text(txui::Point(x + 20, c2.y() + 88), "Enable PAM biometric / password authentication", TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c2.y() + 74, m_pam_auth, active_accent);
    y += 122;

    // Card 3: Session Actions (Lock, Sleep, Restart, Shut Down)
    txui::Rect c3(x, y, cw, 86);
    draw_card(painter, c3, "Session Control");
    const char* actions[] = {"Lock Screen", "Sleep", "Restart", "Shut Down"};
    for (int i = 0; i < 4; ++i) {
        const float64 bx = x + 20 + static_cast<double>(i) * 118.0;
        const bool danger = (i >= 2);
        painter.fill_gradient_rounded_rect(
            txui::Rect(bx, c3.y() + 46, 106, 28), 7.0,
            danger ? txui::Color(160,  40,  40, 200) : txui::Color(35, 42, 60, 200),
            danger ? txui::Color(120,  25,  25, 180) : txui::Color(25, 30, 48, 180)
        );
        painter.draw_text(txui::Point(bx + 12, c3.y() + 54), actions[i],
                          danger ? txui::Color(255, 180, 180, 240) : TXT_PRI, 0.95, true);
    }

    if (!m_session_status_msg.empty()) {
        painter.draw_text(txui::Point(x + 20, c3.y() + 94), m_session_status_msg, active_accent, 0.9);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: Privacy & Security
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::refresh_unverified_apps() {
    m_unverified_apps.clear();
    std::string apps_dir = "/opt/tinexus-apps";
    if (!std::filesystem::exists(apps_dir)) return;

    for (const auto& entry : std::filesystem::directory_iterator(apps_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string path = entry.path().string();
        if (path.ends_with(".sig")) continue;

        int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;

        std::string hash = tinexus::guard::CryptoValidator::compute_sha256_fd(fd);
        
        bool trusted = false;
        std::ifstream overrides("/var/lib/tinexus/trust-overrides.conf");
        if (overrides.is_open()) {
            std::string line;
            while (std::getline(overrides, line)) {
                if (line == hash) { trusted = true; break; }
            }
        }
        
        if (trusted) {
            close(fd);
            continue;
        }

        std::string sig_path = path + ".sig";
        std::vector<uint8_t> sig_bytes;
        std::ifstream sig_file(sig_path, std::ios::binary);
        if (sig_file.is_open()) {
            sig_bytes = std::vector<uint8_t>((std::istreambuf_iterator<char>(sig_file)), std::istreambuf_iterator<char>());
        }

        if (sig_bytes.empty() || !tinexus::guard::CryptoValidator::verify_signature_fd(fd, sig_bytes, "/etc/tinexus/keys/root.pub")) {
            UnverifiedApp app;
            app.name = entry.path().filename().string();
            app.path = path;
            app.hash = hash;
            m_unverified_apps.push_back(app);
        }
        close(fd);
    }
}

void SettingsWidget::trust_app(const std::string& hash) {
    std::ofstream overrides("/var/lib/tinexus/trust-overrides.conf", std::ios::app);
    if (overrides.is_open()) {
        overrides << hash << "\n";
    }
    refresh_unverified_apps();
}

void SettingsWidget::paint_privacy_security_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;

    painter.draw_text(txui::Point(x, y), "Privacy & Security", TXT_PRI, 2.0);
    y += 52;

    // Gatekeeper Card
    txui::Rect c1(x, y, cw, 120.0 + static_cast<double>(m_unverified_apps.size()) * 50.0);
    draw_card(painter, c1, "Application Gatekeeper");
    
    painter.draw_text(txui::Point(x + 20, y + 46), "Allow applications downloaded from:", TXT_SEC, 1.0);
    painter.draw_text(txui::Point(x + 30, y + 66), "- Tinexus Verified Developers (ECDSA P-256)", TXT_PRI, 1.0);
    
    y += 90;

    if (!m_unverified_apps.empty()) {
        painter.fill_rect(txui::Rect(x + 20, y, cw - 40, 1), DIVIDER);
        y += 15;
        
        for (auto& app : const_cast<SettingsWidget*>(this)->m_unverified_apps) {
            painter.draw_text(txui::Point(x + 20, y + 16), "\"" + app.name + "\" was blocked because it is unsigned.", TXT_SEC, 1.0);
            
            app.btn_rect = txui::Rect(x + cw - 130, y + 4, 110, 28);
            painter.fill_gradient_rounded_rect(app.btn_rect, 6.0, 
                app.is_hovered ? txui::Color(100, 100, 120, 200) : txui::Color(60, 60, 80, 200),
                app.is_hovered ? txui::Color(80, 80, 100, 180) : txui::Color(40, 40, 60, 180));
            painter.draw_text(txui::Point(app.btn_rect.x() + 16, app.btn_rect.y() + 8), "Trust App", TXT_PRI, 1.0);
            y += 40;
        }
    } else {
        painter.draw_text(txui::Point(x + 20, y + 10), "All system binaries verified authentic.", SUCCESS, 0.95);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: About
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_about_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;
    const auto current_accent = ACCENT_PALETTE[m_selected_accent_idx];
    const txui::Color active_accent(current_accent.r, current_accent.g, current_accent.b, 255);

    painter.draw_glow(txui::Point(x + 35, y + 10), 10, 50, active_accent);
    painter.draw_text(txui::Point(x, y), "About Tinexus", TXT_PRI, 2.0);
    y += 52;

    // Hero Logo Card
    txui::Rect logo_card(x, y, cw, 128);
    painter.fill_gradient_rounded_rect(logo_card, 18.0,
        txui::Color(16, 18, 38, 240),
        txui::Color(10, 10, 22, 220)
    );
    painter.fill_circle(txui::Point(x + cw * 0.78, y + 60), 80.0,
        txui::Color(current_accent.r, current_accent.g, current_accent.b, 25));
    painter.fill_gradient_rounded_rect(txui::Rect(x + 1, y + 1, 4, 126), 2.0,
        active_accent, darken(current_accent.r, current_accent.g, current_accent.b, 0.8, 120));

    painter.draw_glow(txui::Point(x + 40 + 35, y + 32), 14, 50, active_accent);
    painter.draw_text(txui::Point(x + 28, y + 22), "TINEXUS DESKTOP", active_accent, 3.0);
    painter.draw_text(txui::Point(x + 28, y + 66), "Version 0.1.0  \"Horizon\"",    TXT_PRI, 1.0);
    painter.draw_text(txui::Point(x + 28, y + 86), "Pure C++20 & Wayland Platform", TXT_SEC, 1.0);
    painter.draw_text(txui::Point(x + 28, y + 106),"GPL-2.0-or-later  |  txui Framework", TXT_DIM, 1.0);
    y += 140;

    // System Information Card
    txui::Rect c2(x, y, cw, 178);
    draw_card(painter, c2, "System Specifications");
    draw_row(painter, c2,  52, "OS Kernel",     m_os_version);
    draw_row(painter, c2,  74, "Processor",     m_cpu_model);
    draw_row(painter, c2,  96, "System Memory", m_mem_info);
    draw_row(painter, c2, 118, "Compositor",    "tinexus-comp (wlroots + Vulkan ready)");
    draw_row(painter, c2, 140, "Display Server","Wayland-native");
    draw_row(painter, c2, 162, "Build Target",  "Release x86_64", SUCCESS);
}

} // namespace tinexus::settings_ui
