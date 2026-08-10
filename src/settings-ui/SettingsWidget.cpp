// ─────────────────────────────────────────────────────────────────────────────
// SettingsWidget.cpp  — Tinexus Control Center
// macOS-inspired premium dark settings UI with gradient cards, circles, glow
// ─────────────────────────────────────────────────────────────────────────────
#include "settings/SettingsWidget.hpp"
#include <txui/render/Painter.hpp>
#include <txui/graphics/Color.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Point.hpp>
#include <string>
#include <ctime>
#include <cmath>
#include <filesystem>
#include <fcntl.h>
#include "guard/crypto_validator.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// Design Token Palette — matches docs/05_UI_UX_GUIDELINES.md
// ─────────────────────────────────────────────────────────────────────────────
namespace {

// Backgrounds
constexpr txui::Color BG_BASE      { 8,  8, 12, 255};  // #08080C
constexpr txui::Color BG_SURFACE   {16, 16, 24, 255};  // #101018
constexpr txui::Color BG_ELEVATED  {24, 24, 36, 255};  // #181824
constexpr txui::Color BG_SIDEBAR   {12, 12, 20, 255};

// Accent
constexpr txui::Color ACCENT       {107, 140, 239, 255}; // #6B8CEF
constexpr txui::Color ACCENT_LOW   {107, 140, 239,  35};
constexpr txui::Color ACCENT_MID   {107, 140, 239,  90};

// Gradients for sidebar items
constexpr txui::Color SEL_GRAD_A   { 80, 110, 220,  80};
constexpr txui::Color SEL_GRAD_B   { 50,  70, 180,  40};

// Card gradient
constexpr txui::Color CARD_TOP     {22, 22, 34, 230};
constexpr txui::Color CARD_BOT     {18, 18, 28, 200};

// Text
constexpr txui::Color TXT_PRI      {240, 240, 248, 255};
constexpr txui::Color TXT_SEC      {140, 140, 170, 220};
constexpr txui::Color TXT_DIM      { 80,  80, 100, 180};

// Status
constexpr txui::Color SUCCESS      { 80, 200, 130, 255};
constexpr txui::Color WARNING      {230, 160,  50, 255};
constexpr txui::Color DIVIDER      { 36,  36,  52, 255};

// Glow accent
constexpr txui::Color GLOW_BLUE    {107, 140, 239, 140};

} // namespace

#include <sys/utsname.h>
#include <fstream>
#include <sstream>

namespace tinexus::settings_ui {

using txui::float64;
using txui::uint8;

SettingsWidget::SettingsWidget() {
    m_current_page = SettingsPage::Display;

    // 1. Read OS Version
    struct utsname name;
    if (uname(&name) == 0) {
        m_os_version = std::string(name.sysname) + " " + name.release;
    } else {
        m_os_version = "Unknown Linux";
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

    // 4. Parse TOML Config for System Tab
    std::string config_path = std::string(getenv("HOME")) + "/.config/tinexus/tinexus-settings.toml";
    std::ifstream toml(config_path);
    while (std::getline(toml, line)) {
        if (line.find("screen_timeout") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) m_screen_timeout_min = std::stoi(line.substr(eq + 1));
        } else if (line.find("sleep_after") != std::string::npos) {
            auto eq = line.find('=');
            if (eq != std::string::npos) m_sleep_after_min = std::stoi(line.substr(eq + 1));
        } else if (line.find("power_profile") != std::string::npos) {
            auto start = line.find('"');
            auto end = line.find('"', start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                m_power_profile = line.substr(start + 1, end - start - 1);
            }
        }
    }
    
    refresh_unverified_apps();
}

void SettingsWidget::select_page(SettingsPage page) {
    m_current_page = page;
    mark_needs_paint();
}

txui::Size SettingsWidget::measure_override(const txui::Constraints& c) noexcept {
    return c.constrain(txui::Size(c.max_width, c.max_height));
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
        if (m_hovered_tab == 0) { select_page(SettingsPage::Display); return true; }
        else if (m_hovered_tab == 1) { select_page(SettingsPage::Personalization); return true; }
        else if (m_hovered_tab == 2) { select_page(SettingsPage::System); return true; }
        else if (m_hovered_tab == 3) { select_page(SettingsPage::PrivacySecurity); return true; }
        else if (m_hovered_tab == 4) { select_page(SettingsPage::About); return true; }
        return false;
    }
    
    // Pass event to PrivacySecurity page if active
    if (m_current_page == SettingsPage::PrivacySecurity && event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            double px = event.pointer.x;
            double py = event.pointer.y;
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

void SettingsWidget::layout_override(const txui::Rect& f) noexcept {
    // 20px padding around content area for macOS-like generous whitespace
    constexpr double CONTENT_PADDING = 20.0;
    m_sidebar_rect = txui::Rect(f.x(), f.y(), SIDEBAR_W, f.height());
    m_content_rect = txui::Rect(
        f.x() + SIDEBAR_W + CONTENT_PADDING,
        f.y() + CONTENT_PADDING,
        f.width() - SIDEBAR_W - CONTENT_PADDING * 2.0,
        f.height() - CONTENT_PADDING * 2.0
    );
}


// ─────────────────────────────────────────────────────────────────────────────
// Paint root
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_override(txui::Painter& painter) const noexcept {
    // ── Full window gradient background
    painter.fill_gradient_rect(frame(),
        txui::Color( 6,  6, 14, 255),
        txui::Color(10, 10, 18, 255));

    // ── Sidebar aurora glow (top-left atmosphere)
    painter.fill_circle(
        txui::Point(SIDEBAR_W * 0.4, frame().height() * 0.15),
        SIDEBAR_W * 0.9,
        txui::Color(80, 60, 180, 18)
    );

    // ── Sidebar background gradient
    painter.fill_gradient_rect(m_sidebar_rect,
        txui::Color(14, 14, 22, 255),
        txui::Color(10, 10, 18, 255));

    // ── Sidebar title + logo mark
    // Small accent circle (logo dot)
    painter.fill_circle(
        txui::Point(m_sidebar_rect.x() + 20, m_sidebar_rect.y() + 30),
        6.0, ACCENT
    );
    painter.draw_text(
        txui::Point(m_sidebar_rect.x() + 32, m_sidebar_rect.y() + 22),
        "Settings", TXT_PRI, 2.0
    );

    // ── Sidebar nav items
    paint_sidebar(painter);

    // ── Vertical separator line with gradient (top-bright, fade to transparent)
    for (int i = 0; i < static_cast<int>(frame().height()); ++i) {
        const double t = static_cast<double>(i) / frame().height();
        const uint8 alpha = static_cast<uint8>(50.0 * (1.0 - t * t));
        painter.fill_rect(
            txui::Rect(m_sidebar_rect.right() - 1, frame().y() + i, 1, 1),
            txui::Color(107, 140, 239, alpha)
        );
    }

    // ── Content background
    painter.fill_gradient_rect(m_content_rect,
        txui::Color( 8,  8, 14, 255),
        txui::Color( 6,  6, 12, 255));

    // ── Dispatch page
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
    
    // ── Search Bar
    // Title is drawn at sy + 22. The text (size 2.0) has descenders reaching ~sy + 36.
    // Placing search bar at sy + 90 guarantees a massive >50px visual gap.
    const float64 search_y = sy + 90.0;
    const float64 search_x = m_sidebar_rect.x() + 10;
    const float64 search_w = SIDEBAR_W - 20;
    // Base dark layer
    painter.fill_rounded_rect(txui::Rect(search_x, search_y, search_w, 32.0), 8.0, txui::Color(255, 255, 255, 12));
    // Inner slightly darker layer to create a 1px border effect
    painter.fill_rounded_rect(txui::Rect(search_x + 1, search_y + 1, search_w - 2, 30.0), 7.0, txui::Color(0, 0, 0, 80));
    
    // Search icon (magnifying glass)
    const float64 icon_cx = search_x + 16.0;
    const float64 icon_cy = search_y + 16.0;
    painter.draw_circle(txui::Point(icon_cx, icon_cy), 5.0, 1.5, TXT_SEC);
    painter.fill_rect(txui::Rect(icon_cx + 3.0, icon_cy + 3.0, 4.0, 1.5), TXT_SEC); // simple handle

    painter.draw_text(txui::Point(search_x + 32, search_y + 8), "Search", TXT_SEC, 1.0);

    // ── Sidebar Items
    const float64 items_start_y = search_y + 48.0;
    
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

    if (active) {
        // Glowing gradient selection pill
        painter.fill_gradient_rounded_rect(
            txui::Rect(x, y + 2, iw, ITEM_H - 4),
            10.0,
            SEL_GRAD_A, SEL_GRAD_B
        );
        // Left accent bar
        painter.fill_gradient_rounded_rect(
            txui::Rect(x + 2, y + 8, 3, ITEM_H - 16),
            1.5,
            ACCENT, txui::Color(80, 110, 220, 140)
        );
    } else {
        // Hover state
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
        painter.fill_gradient_rounded_rect(tile_rect, 6.0, txui::Color(255, 80, 150, 255), txui::Color(240, 40, 100, 255));
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

    // Label
    const txui::Color col = active ? TXT_PRI : TXT_SEC;
    painter.draw_text(txui::Point(x + 48, y + ITEM_H * 0.5 - 7), label, col, 1.0);
}

void SettingsWidget::draw_icon_display(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    // Monitor frame (outer solid)
    painter.fill_rounded_rect(txui::Rect(tx+5, ty+6, 18, 12), 2.0, txui::Color(255, 255, 255, 255));
    // Screen (inner, matches tile color)
    painter.fill_rect(txui::Rect(tx+7, ty+8, 14, 8), txui::Color(80, 150, 255, 255));
    // Stand
    painter.fill_rect(txui::Rect(tx+12, ty+18, 4, 3), txui::Color(255, 255, 255, 255));
    // Base
    painter.fill_rect(txui::Rect(tx+8, ty+21, 12, 2.0), txui::Color(255, 255, 255, 255));
}

void SettingsWidget::draw_icon_personalization(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    // Palette body
    painter.fill_circle(txui::Point(tx+14, ty+14), 8.0, txui::Color(255, 255, 255, 255));
    // Thumb hole
    painter.fill_circle(txui::Point(tx+18, ty+15), 2.5, txui::Color(240, 60, 90, 255));
    // Paint dots
    painter.fill_circle(txui::Point(tx+10, ty+11), 1.5, txui::Color(240, 60, 90, 255));
    painter.fill_circle(txui::Point(tx+10, ty+17), 1.5, txui::Color(240, 60, 90, 255));
    painter.fill_circle(txui::Point(tx+14, ty+9), 1.5, txui::Color(240, 60, 90, 255));
}

void SettingsWidget::draw_icon_system(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    // Chip body
    painter.fill_rounded_rect(txui::Rect(tx+7, ty+7, 14, 14), 2.0, txui::Color(255, 255, 255, 255));
    // Inner core
    painter.fill_rect(txui::Rect(tx+10, ty+10, 8, 8), txui::Color(110, 110, 120, 255));
    // Pins top/bottom
    painter.fill_rect(txui::Rect(tx+9, ty+4, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+13, ty+4, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+17, ty+4, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+9, ty+21, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+13, ty+21, 2, 3), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+17, ty+21, 2, 3), txui::Color(255, 255, 255, 255));
    // Pins left/right
    painter.fill_rect(txui::Rect(tx+4, ty+9, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+4, ty+13, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+4, ty+17, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+21, ty+9, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+21, ty+13, 3, 2), txui::Color(255, 255, 255, 255));
    painter.fill_rect(txui::Rect(tx+21, ty+17, 3, 2), txui::Color(255, 255, 255, 255));
}

void SettingsWidget::draw_icon_privacy(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    // left leg
    painter.fill_rect(txui::Rect(tx+9, ty+6, 2, 6), txui::Color(255, 255, 255, 255));
    // right leg
    painter.fill_rect(txui::Rect(tx+17, ty+6, 2, 6), txui::Color(255, 255, 255, 255));
    // top bar
    painter.fill_rounded_rect(txui::Rect(tx+9, ty+4, 10, 4), 1.5, txui::Color(255, 255, 255, 255));
    // inner transparent part of shackle (if top bar is too thick, this creates the U shape)
    painter.fill_rect(txui::Rect(tx+11, ty+6, 6, 6), txui::Color(240, 160, 40, 255));
    // Lock body
    painter.fill_rounded_rect(txui::Rect(tx+7, ty+12, 14, 10), 2.0, txui::Color(255, 255, 255, 255));
    // Keyhole
    painter.fill_circle(txui::Point(tx+14, ty+16), 1.5, txui::Color(240, 160, 40, 255));
    painter.fill_rect(txui::Rect(tx+13, ty+17, 2, 3), txui::Color(240, 160, 40, 255));
}

void SettingsWidget::draw_icon_about(txui::Painter& painter, txui::float64 tx, txui::float64 ty) const noexcept {
    painter.fill_circle(txui::Point(tx+14, ty+14), 8.0, txui::Color(255, 255, 255, 255));
    // "i" dot
    painter.fill_rect(txui::Rect(tx+13, ty+9, 2, 2), txui::Color(40, 180, 160, 255));
    // "i" body
    painter.fill_rect(txui::Rect(tx+13, ty+12, 2, 5), txui::Color(40, 180, 160, 255));
}

// ─────────────────────────────────────────────────────────────────────────────
// Card helpers
// ─────────────────────────────────────────────────────────────────────────────
static void draw_card(txui::Painter& painter, const txui::Rect& r, const std::string& title) {
    // ── Translucent Material Simulation (Drop shadow behind the card)
    painter.fill_rounded_rect(
        txui::Rect(r.x() - 2, r.y() + 4, r.width() + 4, r.height() + 4),
        16.0, txui::Color(0, 0, 0, 180)
    );
    
    // Gradient card background
    painter.fill_gradient_rounded_rect(r, 14.0, CARD_TOP, CARD_BOT);
    // Subtle top highlight
    painter.fill_rounded_rect(
        txui::Rect(r.x() + 1, r.y() + 1, r.width() - 2, 1),
        0.5, txui::Color(255, 255, 255, 30)
    );
    // Title
    painter.draw_text(txui::Point(r.x() + 20, r.y() + 16), title, TXT_PRI, 1.0);
    // Divider
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
    // Right-align value
    const float64 val_x = card.x() + card.width() - static_cast<float64>(value.size()) * 8.0 - 20.0;
    painter.draw_text(txui::Point(val_x, card.y() + row_y), value, value_color, 1.0);
}

static void draw_toggle(txui::Painter& painter, float64 tx, float64 ty, bool on) {
    // Track
    painter.fill_gradient_rounded_rect(
        txui::Rect(tx, ty, 42, 22), 11.0,
        on ? txui::Color(107, 140, 239, 220) : txui::Color(40, 40, 60, 220),
        on ? txui::Color( 70, 110, 200, 180) : txui::Color(30, 30, 50, 200)
    );
    // Knob
    const float64 knob_x = on ? tx + 22 : tx + 2;
    painter.fill_circle(txui::Point(knob_x + 9, ty + 11), 9.0,
                        txui::Color(240, 240, 255, 240));
    if (on) {
        painter.draw_glow(txui::Point(knob_x + 9, ty + 11), 9.0, 18.0, ACCENT);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: Display
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_display_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;

    // Page title with accent glow
    painter.draw_glow(txui::Point(x + 35, y + 10), 10, 50, GLOW_BLUE);
    painter.draw_text(txui::Point(x, y), "Display", TXT_PRI, 2.0);
    y += 52;

    // Card 1: Monitor
    txui::Rect c1(x, y, cw, 120);
    draw_card(painter, c1, "Monitor");
    draw_row(painter, c1, 52, "Resolution",   "1920 x 1080");
    draw_row(painter, c1, 76, "Refresh Rate", "60 Hz",        SUCCESS);
    draw_row(painter, c1, 100,"Output",       "HDMI-A-1");
    y += 130;

    // Card 2: Scale
    txui::Rect c2(x, y, cw, 88);
    draw_card(painter, c2, "Scale & Layout");
    draw_row(painter, c2, 52, "Display Scale",  "100%  (1x)");
    draw_row(painter, c2, 74, "Orientation",    "Landscape");
    y += 98;

    // Card 3: Night Light
    txui::Rect c3(x, y, cw, 72);
    draw_card(painter, c3, "Night Light");
    painter.draw_text(txui::Point(x + 20, c3.y() + 52), "Reduce blue light after sunset",
                      TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c3.y() + 38, false);
    y += 82;

    // Card 4: VRR
    txui::Rect c4(x, y, cw, 68);
    draw_card(painter, c4, "Variable Refresh Rate (VRR)");
    painter.draw_text(txui::Point(x + 20, c4.y() + 50), "FreeSync / G-Sync compatible",
                      TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c4.y() + 44, false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: Personalization
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_personalization_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;

    painter.draw_text(txui::Point(x, y), "Personalization", TXT_PRI, 2.0);
    y += 52;

    // Card 1: Accent Colors
    txui::Rect c1(x, y, cw, 100);
    draw_card(painter, c1, "Accent Color");
    const struct { uint8 r, g, b; } palette[] = {
        {107, 140, 239},  // Blue (active)
        { 80, 200, 130},  // Green
        {230, 130,  60},  // Orange
        {200,  80, 170},  // Pink
        {220, 190,  50},  // Yellow
        { 80, 190, 220},  // Teal
    };
    for (int i = 0; i < 6; ++i) {
        const float64 cx_ = x + 22 + i * 50.0;
        const float64 cy_ = c1.y() + 66;
        painter.fill_circle(txui::Point(cx_, cy_), 16.0,
            txui::Color(palette[i].r, palette[i].g, palette[i].b, 220));
        if (i == 0) {
            // Active ring
            painter.draw_circle(txui::Point(cx_, cy_), 18.0, 2.0,
                txui::Color(palette[i].r, palette[i].g, palette[i].b, 200));
            painter.draw_glow(txui::Point(cx_, cy_), 16.0, 26.0,
                txui::Color(palette[i].r, palette[i].g, palette[i].b, 200));
        }
    }
    y += 110;

    // Card 2: Theme
    txui::Rect c2(x, y, cw, 80);
    draw_card(painter, c2, "Appearance");
    constexpr float64 pill_w = 96, pill_h = 28;
    const float64 py = c2.y() + 46;
    painter.fill_gradient_rounded_rect(txui::Rect(x + 20, py, pill_w, pill_h), 8.0,
        ACCENT, txui::Color(70, 110, 200, 200));
    painter.draw_text(txui::Point(x + 43, py + 8), "Dark", TXT_PRI, 1.0);
    painter.fill_rounded_rect(txui::Rect(x + 20 + pill_w + 8, py, pill_w, pill_h), 8.0,
        txui::Color(30, 30, 44, 200));
    painter.draw_text(txui::Point(x + 20 + pill_w + 24, py + 8), "Light", TXT_SEC, 1.0);
    y += 90;

    // Card 3: Wallpaper
    txui::Rect c3(x, y, cw, 68);
    draw_card(painter, c3, "Wallpaper");
    draw_row(painter, c3, 52, "Current", "tinexus-default.jpg");
    y += 78;

    // Card 4: Font
    txui::Rect c4(x, y, cw, 68);
    draw_card(painter, c4, "Font");
    draw_row(painter, c4, 52, "System Font", "Inter  14pt");
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: System
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_system_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;

    painter.draw_text(txui::Point(x, y), "System", TXT_PRI, 2.0);
    y += 52;

    // Card 1: Power
    txui::Rect c1(x, y, cw, 112);
    draw_card(painter, c1, "Power");
    draw_row(painter, c1, 52, "Screen Timeout",  std::to_string(m_screen_timeout_min) + " minutes");
    draw_row(painter, c1, 74, "Sleep After",     std::to_string(m_sleep_after_min) + " minutes");
    draw_row(painter, c1, 96, "Power Profile",   m_power_profile,   SUCCESS);
    y += 122;

    // Card 2: Security
    txui::Rect c2(x, y, cw, 112);
    draw_card(painter, c2, "Security & Privacy");
    painter.draw_text(txui::Point(x + 20, c2.y() + 52), "Lock screen on sleep", TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c2.y() + 38, true);
    painter.draw_text(txui::Point(x + 20, c2.y() + 88), "PAM authentication",   TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c2.y() + 74, true);
    y += 122;

    // Card 3: Session actions
    txui::Rect c3(x, y, cw, 80);
    draw_card(painter, c3, "Session");
    const char* actions[] = {"Lock", "Sleep", "Restart", "Shut Down"};
    for (int i = 0; i < 4; ++i) {
        const float64 bx = x + 20 + i * 118.0;
        const bool danger = (i == 3);
        painter.fill_gradient_rounded_rect(
            txui::Rect(bx, c3.y() + 48, 106, 26), 7.0,
            danger ? txui::Color(160,  40,  40, 200) : txui::Color(30, 30, 46, 200),
            danger ? txui::Color(120,  25,  25, 180) : txui::Color(24, 24, 38, 180)
        );
        painter.draw_text(txui::Point(bx + 14, c3.y() + 55), actions[i],
                          danger ? txui::Color(255, 180, 180, 240) : TXT_PRI, 1.0);
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

    // Card 1: Gatekeeper
    txui::Rect c1(x, y, cw, 120.0 + static_cast<double>(m_unverified_apps.size()) * 50.0);
    draw_card(painter, c1, "Security");
    
    painter.draw_text(txui::Point(x + 20, y + 46), "Allow applications downloaded from:", TXT_SEC, 1.0);
    painter.draw_text(txui::Point(x + 30, y + 66), "- Tinexus Verified Developers", TXT_PRI, 1.0);
    
    y += 90;

    if (!m_unverified_apps.empty()) {
        painter.fill_rect(txui::Rect(x + 20, y, cw - 40, 1), DIVIDER);
        y += 15;
        
        for (auto& app : const_cast<SettingsWidget*>(this)->m_unverified_apps) {
            painter.draw_text(txui::Point(x + 20, y + 16), "\"" + app.name + "\" was blocked because it is not from an identified developer.", TXT_SEC, 1.0);
            
            app.btn_rect = txui::Rect(x + cw - 130, y + 4, 110, 28);
            painter.fill_gradient_rounded_rect(app.btn_rect, 6.0, 
                app.is_hovered ? txui::Color(100, 100, 120, 200) : txui::Color(60, 60, 80, 200),
                app.is_hovered ? txui::Color(80, 80, 100, 180) : txui::Color(40, 40, 60, 180));
            painter.draw_text(txui::Point(app.btn_rect.x() + 16, app.btn_rect.y() + 8), "Open Anyway", TXT_PRI, 1.0);
            
            y += 40;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Page: About
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_about_page(txui::Painter& painter, const txui::Rect& area) const noexcept {
    const float64 x  = area.x() + 40;
    float64 y         = area.y() + 32;
    const float64 cw  = area.width() - 80;

    painter.draw_text(txui::Point(x, y), "About", TXT_PRI, 2.0);
    y += 52;

    // Hero logo card
    txui::Rect logo_card(x, y, cw, 128);
    painter.fill_gradient_rounded_rect(logo_card, 18.0,
        txui::Color(16, 18, 38, 240),
        txui::Color(10, 10, 22, 220)
    );
    // Accent aurora inside card
    painter.fill_circle(txui::Point(x + cw * 0.78, y + 60), 80.0,
        txui::Color(80, 60, 180, 20));
    // Left accent bar (gradient)
    painter.fill_gradient_rounded_rect(txui::Rect(x + 1, y + 1, 4, 126), 2.0,
        ACCENT, txui::Color(70, 110, 200, 120));

    // TINEXUS wordmark
    painter.draw_glow(txui::Point(x + 40 + 35, y + 32), 14, 50, GLOW_BLUE);
    painter.draw_text(txui::Point(x + 28, y + 22), "TINEXUS DESKTOP", ACCENT, 3.0);
    painter.draw_text(txui::Point(x + 28, y + 66), "Version 0.1.0  \"Horizon\"",    TXT_PRI, 1.0);
    painter.draw_text(txui::Point(x + 28, y + 86), "Build with Linux & Wayland", TXT_SEC, 1.0);
    painter.draw_text(txui::Point(x + 28, y + 106),"GPL-2.0-or-later  |  SDK Apache-2.0",       TXT_DIM, 1.0);
    y += 140;

    // System info card
    txui::Rect c2(x, y, cw, 178);
    draw_card(painter, c2, "System Information");
    draw_row(painter, c2,  52, "OS Kernel",     m_os_version);
    draw_row(painter, c2,  74, "Processor",     m_cpu_model);
    draw_row(painter, c2,  96, "Memory (RAM)",  m_mem_info);
    draw_row(painter, c2, 118, "Compositor",    "tinexus-comp v0.1.0 (wlroots + Vulkan)");
    draw_row(painter, c2, 140, "Architecture",  "x86_64  Wayland-native");
    draw_row(painter, c2, 162, "Build",         "Release  (GCC C++20)", SUCCESS);
}

} // namespace tinexus::settings_ui
