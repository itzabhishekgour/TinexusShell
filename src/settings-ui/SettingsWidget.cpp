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
        }

        if (new_hover != m_hovered_tab) {
            m_hovered_tab = new_hover;
            mark_needs_paint();
        }
        return false;
    } else if (event.type == txui::EventType::PointerButtonPress && event.pointer.button == txui::MouseButton::Left) {
        if (m_hovered_tab == 0) { select_page(SettingsPage::Display); return true; }
        else if (m_hovered_tab == 1) { select_page(SettingsPage::Personalization); return true; }
        else if (m_hovered_tab == 2) { select_page(SettingsPage::System); return true; }
        else if (m_hovered_tab == 3) { select_page(SettingsPage::About); return true; }
        return false;
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
        case SettingsPage::About:           paint_about_page(painter, m_content_rect);           break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Sidebar
// ─────────────────────────────────────────────────────────────────────────────
void SettingsWidget::paint_sidebar(txui::Painter& painter) const noexcept {
    paint_sidebar_item(painter, "Display",         SettingsPage::Display,         ITEM_Y0);
    paint_sidebar_item(painter, "Personalization", SettingsPage::Personalization, ITEM_Y0 + ITEM_H + 4);
    paint_sidebar_item(painter, "System",          SettingsPage::System,          ITEM_Y0 + (ITEM_H + 4) * 2);
    paint_sidebar_item(painter, "About",           SettingsPage::About,           ITEM_Y0 + (ITEM_H + 4) * 3);
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
        else if (page == SettingsPage::About) tab_idx = 3;

        if (m_hovered_tab == tab_idx) {
            painter.fill_rounded_rect(
                txui::Rect(x, y + 2, iw, ITEM_H - 4),
                10.0,
                txui::Color(255, 255, 255, 10)
            );
        }
    }

    // Icon circle
    const float64 icon_cx = x + 16;
    const float64 icon_cy = y + ITEM_H * 0.5;
    if (active) {
        painter.fill_circle(txui::Point(icon_cx, icon_cy), 9.0, ACCENT_LOW);
        painter.draw_circle(txui::Point(icon_cx, icon_cy), 9.0, 1.0, ACCENT_MID);
    } else {
        painter.draw_circle(txui::Point(icon_cx, icon_cy), 8.0, 1.0,
                            txui::Color(100, 100, 140, 80));
    }

    // Label
    const txui::Color col = active ? TXT_PRI : TXT_SEC;
    painter.draw_text(txui::Point(x + 30, y + ITEM_H * 0.5 - 7), label, col, 1.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Card helpers
// ─────────────────────────────────────────────────────────────────────────────
static void draw_card(txui::Painter& painter, const txui::Rect& r, const std::string& title) {
    // Gradient card background
    painter.fill_gradient_rounded_rect(r, 14.0, CARD_TOP, CARD_BOT);
    // Subtle top highlight
    painter.fill_rounded_rect(
        txui::Rect(r.x() + 1, r.y() + 1, r.width() - 2, 1),
        0.5, txui::Color(255, 255, 255, 12)
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
    txui::Rect c3(x, y, cw, 68);
    draw_card(painter, c3, "Night Light");
    painter.draw_text(txui::Point(x + 20, c3.y() + 50), "Reduce blue light after sunset",
                      TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c3.y() + 44, false);
    y += 78;

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
    txui::Rect c2(x, y, cw, 94);
    draw_card(painter, c2, "Security & Privacy");
    painter.draw_text(txui::Point(x + 20, c2.y() + 52), "Lock screen on sleep", TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c2.y() + 46, true);
    painter.draw_text(txui::Point(x + 20, c2.y() + 76), "PAM authentication",   TXT_SEC, 1.0);
    draw_toggle(painter, x + cw - 58, c2.y() + 70, true);
    y += 104;

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
