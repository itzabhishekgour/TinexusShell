// ─────────────────────────────────────────────────────────────────────────────
// LockWidget.cpp — Tinexus Lock Screen
// Premium Glassmorphism Design: Ultra-fast <1ms render time, clean geometry
// ─────────────────────────────────────────────────────────────────────────────
#include "lock/LockWidget.hpp"
#include <common/TinexusLogo.hpp>
#include <txui/render/Painter.hpp>
#include <txui/graphics/Color.hpp>
#include <txui/math/Point.hpp>
#include <txui/math/Rect.hpp>
#include <ctime>
#include <cstring>
#include <string>
#include <algorithm>

namespace tinexus::lock {

namespace {

// Background gradient (Deep Midnight Nebula)
constexpr txui::Color BG_TOP      {  8,  12,  32, 255}; // Deep cosmic navy
constexpr txui::Color BG_BOT      {  2,   3,  10, 255}; // Pure abyss

// Card & Glass accents
constexpr txui::Color CARD_BG     { 20,  25,  45, 140}; // Frosted glass card
constexpr txui::Color CARD_BORDER {100, 130, 220,  40}; // Card border

// Input pill
constexpr txui::Color PILL_BG     { 10,  15,  30, 200}; // Dark glass pill
constexpr txui::Color PILL_BORDER {120, 150, 255,  80}; // Glowing accent border

// Text tokens
constexpr txui::Color TIME_COLOR  {255, 255, 255, 255}; // Crisp white
constexpr txui::Color DATE_COLOR  {170, 185, 220, 210}; // Soft slate
constexpr txui::Color USER_COLOR  {220, 230, 255, 240}; // Soft white
constexpr txui::Color HINT_COLOR  {130, 145, 180, 160}; // Subtle hint
constexpr txui::Color ACCENT      {110, 150, 255, 255}; // Tinexus neon blue
constexpr txui::Color ERROR_COL   {255,  90,  90, 255}; // Coral red

} // namespace

void LockWidget::add_password_char(char c) {
    if (m_lockout_seconds > 0) return;
    m_password += c;
    mark_needs_paint();
}

void LockWidget::remove_password_char() {
    if (m_lockout_seconds > 0) return;
    if (!m_password.empty()) {
        m_password.pop_back();
        mark_needs_paint();
    }
}

void LockWidget::clear_password() {
    m_password.clear();
    mark_needs_paint();
}

void LockWidget::trigger_shake_animation() {
    clear_password();
    m_shaking     = true;
    m_shake_frame = 0;
    m_shake_offset = 0;
    mark_needs_paint();
}

bool LockWidget::advance_shake() noexcept {
    if (!m_shaking) return false;

    // 8-frame sequence: alternating ±10px displacement (right, left, right…)
    // Frame offsets: [ +10, -10, +8, -8, +5, -5, +2, -2, 0 ]
    static constexpr int kOffsets[] = { 10, -10, 8, -8, 5, -5, 2, -2, 0 };
    constexpr int kFrameCount = static_cast<int>(
        sizeof(kOffsets) / sizeof(kOffsets[0]));

    if (m_shake_frame < kFrameCount) {
        m_shake_offset = kOffsets[m_shake_frame];
        ++m_shake_frame;
        mark_needs_paint();
        return true;
    }

    // Animation complete
    m_shaking      = false;
    m_shake_offset = 0;
    m_shake_frame  = 0;
    mark_needs_paint();
    return false;
}

void LockWidget::set_lockout(int seconds) {
    m_lockout_seconds = seconds;
    mark_needs_paint();
}

txui::Size LockWidget::measure_override(const txui::Constraints& constraints) noexcept {
    return txui::Size(constraints.max_width, constraints.max_height);
}

void LockWidget::paint_override(txui::Painter& painter) const noexcept {
    const double W = frame().width();
    const double H = frame().height();
    const double cx = W / 2.0;

    // ── 1. Cosmic Background Gradient
    painter.fill_gradient_rect(frame(), BG_TOP, BG_BOT);

    // ── 2. Time & Date block
    time_t now = time(nullptr);
    struct tm t_buf;
    struct tm* t = (localtime_r(&now, &t_buf) != nullptr) ? &t_buf : nullptr;
    char time_buf[8]  = "??:??";
    char date_buf[48] = "";
    if (t) {
        strftime(time_buf, sizeof(time_buf), "%H:%M", t);
        strftime(date_buf, sizeof(date_buf), "%A, %B %d", t);
    }

    // Large Time Text (scale = 4 → 32px per char)
    constexpr double TIME_SCALE = 4.0;
    const double time_char_w = 8.0 * TIME_SCALE;
    const double time_w      = 5.0 * time_char_w; // "HH:MM"
    const double time_y      = H * 0.20;
    const double time_x      = cx - time_w * 0.5;
    painter.draw_text(txui::Point(time_x, time_y), time_buf, TIME_COLOR, TIME_SCALE);

    // Date Subtitle
    constexpr double DATE_SCALE = 1.0;
    const double date_char_w = 8.0 * DATE_SCALE;
    const double date_len    = static_cast<double>(strlen(date_buf));
    const double date_x      = cx - (date_len * date_char_w) * 0.5;
    const double date_y      = time_y + 16.0 * TIME_SCALE + 12.0;
    painter.draw_text(txui::Point(date_x, date_y), date_buf, DATE_COLOR, DATE_SCALE);

    // ── 3. Central Login Card
    const double card_w = 360.0;
    const double card_h = 240.0;
    const double card_x = cx - card_w * 0.5;
    const double card_y = H * 0.50;
    const txui::Rect card_rect(card_x, card_y, card_w, card_h);

    // Card background & sleek border
    painter.fill_rounded_rect(
        txui::Rect(card_x - 1, card_y - 1, card_w + 2, card_h + 2),
        20.0, CARD_BORDER);
    painter.fill_rounded_rect(card_rect, 19.0, CARD_BG);

    // ── 4. Brand Logo / Avatar inside Card
    const double avatar_y = card_y + 44.0;
    auto logo_buf = logo::get_logo(64);
    if (logo_buf.is_valid()) {
        painter.draw_glow(txui::Point(cx, avatar_y), 14.0, 32.0, txui::Color(124, 58, 237, 75));
        constexpr double logo_sz = 48.0;
        painter.draw_image(txui::Rect(cx - logo_sz * 0.5, avatar_y - logo_sz * 0.5, logo_sz, logo_sz),
                           logo_buf.pixels, logo_buf.width, logo_buf.height);
    } else {
        painter.fill_circle(txui::Point(cx, avatar_y), 24.0, txui::Color(255, 255, 255, 25));
        painter.draw_circle(txui::Point(cx, avatar_y), 24.0, 1.5, txui::Color(255, 255, 255, 60));
        painter.fill_circle(txui::Point(cx, avatar_y - 7.0), 8.0, txui::Color(200, 215, 255, 200));
        painter.fill_circle(txui::Point(cx, avatar_y + 14.0), 12.0, txui::Color(200, 215, 255, 140));
    }

    // User greeting
    const std::string user_name = "Tinexus User";
    const double user_x = cx - (static_cast<double>(user_name.size()) * 8.0) * 0.5;
    painter.draw_text(txui::Point(user_x, avatar_y + 36.0), user_name, USER_COLOR, 1.0);

    // ── 5. Glassmorphism Password Input Pill (with shake offset when auth fails)
    const double pw_w = 280.0;
    const double pw_h = 44.0;
    const double pw_x = cx - pw_w * 0.5 + static_cast<double>(m_shake_offset);
    const double pw_y = card_y + 140.0;
    const txui::Rect pw_rect(pw_x, pw_y, pw_w, pw_h);

    // Pill border: use ERROR_COL while shaking, accent otherwise
    const txui::Color border_color = m_shaking ? ERROR_COL
        : ((!m_password.empty() || m_caret_visible) ? PILL_BORDER : CARD_BORDER);
    painter.fill_rounded_rect(
        txui::Rect(pw_x - 1, pw_y - 1, pw_w + 2, pw_h + 2),
        23.0, border_color);
    painter.fill_rounded_rect(pw_rect, 22.0, PILL_BG);

    // Password content / status
    if (m_lockout_seconds > 0) {
        std::string msg = "Locked out: " + std::to_string(m_lockout_seconds) + "s";
        const double msg_x = cx - (static_cast<double>(msg.size()) * 8.0) * 0.5;
        painter.draw_text(txui::Point(msg_x, pw_y + 14.0), msg, ERROR_COL, 1.0);
    } else if (!m_password.empty()) {
        // Password dots
        const double dot_r   = 4.5;
        const double spacing = 14.0;
        const double total_w = static_cast<double>(m_password.size()) * spacing;
        double dot_x = cx - total_w * 0.5 + spacing * 0.5;
        for (size_t i = 0; i < m_password.size(); ++i) {
            painter.fill_circle(txui::Point(dot_x, pw_y + pw_h * 0.5), dot_r, ACCENT);
            dot_x += spacing;
        }
        // Blinking caret right after dots
        if (m_caret_visible) {
            painter.fill_rect(txui::Rect(dot_x + 2.0, pw_y + 12.0, 2.0, 20.0), ACCENT);
        }
    } else {
        // Hint text — no caret shown here (caret appears after password dots when typing)
        const std::string hint = "Press Enter to unlock";
        const double hint_x = cx - (static_cast<double>(hint.size()) * 7.2) * 0.5;
        painter.draw_text(txui::Point(hint_x, pw_y + 14.0), hint, HINT_COLOR, 1.0);
    }

    // ── 6. Bottom info bar
    const double bottom_y = H - 32.0;
    const std::string bottom_hint = "Tinexus Platform  •  Horizon v0.1";
    const double bottom_x = cx - (static_cast<double>(bottom_hint.size()) * 8.0) * 0.5;
    painter.draw_text(txui::Point(bottom_x, bottom_y), bottom_hint,
                      txui::Color(140, 150, 180, 80), 1.0);
}

} // namespace tinexus::lock
