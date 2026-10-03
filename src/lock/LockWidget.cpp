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

LockWidget::LockWidget() {
    m_input = txui::make_ref<txui::TextInput>("Press Enter to unlock");
    m_input->set_secure_mode(true);
    m_input->set_corner_radius(22.0); // Pill shape
    m_input->set_font_size(15.0);
    m_input->set_focused(true);
    add_child(m_input);
}

void LockWidget::add_password_char(char c) {
    if (m_lockout_seconds > 0) return;
    m_password += c;
    if (m_input) {
        m_input->set_text(m_password);
        m_input->set_caret_position(m_password.size());
    }
    mark_needs_paint();
}

void LockWidget::remove_password_char() {
    if (m_lockout_seconds > 0) return;
    if (!m_password.empty()) {
        m_password.pop_back();
        if (m_input) {
            m_input->set_text(m_password);
            m_input->set_caret_position(m_password.size());
        }
        mark_needs_paint();
    }
}

void LockWidget::clear_password() {
    m_password.clear();
    if (m_input) {
        m_input->set_text("");
        m_input->set_caret_position(0);
    }
    mark_needs_paint();
}

void LockWidget::trigger_shake_animation() {
    clear_password();
    m_shaking      = true;
    m_shake_frame  = 0;
    m_shake_offset = 0;
    if (m_input) {
        // UX Rule: The coral-red error border is active during the dynamic shake sequence (approx 540ms),
        // giving instant visual feedback for the failed attempt.
        m_input->set_error(true);
    }
    mark_needs_paint();
}

bool LockWidget::advance_shake() noexcept {
    if (!m_shaking) return false;

    // 8-frame sequence: alternating ±10px displacement (right, left, right…)
    // Frame offsets: [ +10, -10, +8, -8, +5, -5, +2, -2, 0 ]
    static constexpr int kOffsets[] = { 10, -10, 8, -8, 5, -5, 2, -2, 0 };
    constexpr int kFrameCount = static_cast<int>(sizeof(kOffsets) / sizeof(kOffsets[0]));

    if (m_shake_frame < kFrameCount) {
        m_shake_offset = kOffsets[m_shake_frame];
        ++m_shake_frame;
        mark_needs_layout();
        mark_needs_paint();
        return true;
    }

    // Animation complete:
    // When the shake animation ends, clear the error flag on the input widget.
    // If lockout follows, it is presented as a distinct disabled/countdown state, not a lingering red border.
    m_shaking      = false;
    m_shake_offset = 0;
    m_shake_frame  = 0;
    if (m_input) {
        m_input->set_error(false);
    }
    mark_needs_layout();
    mark_needs_paint();
    return false;
}

void LockWidget::set_lockout(int seconds) {
    m_lockout_seconds = seconds;
    if (m_input) {
        if (seconds > 0) {
            m_input->set_text("");
            m_input->set_placeholder("Locked out: wait " + std::to_string(seconds) + "s");
            m_input->set_focused(false);
            m_input->set_error(false);
            m_input->set_enabled(false);
        } else {
            m_input->set_placeholder("Press Enter to unlock");
            m_input->set_enabled(true);
            m_input->set_focused(true);
        }
    }
    mark_needs_paint();
}

txui::Size LockWidget::measure_override(const txui::Constraints& constraints) noexcept {
    return txui::Size(constraints.max_width, constraints.max_height);
}

void LockWidget::layout_override(const txui::Rect& frame) noexcept {
    const double W = frame.width();
    const double H = frame.height();
    const double cx = frame.x() + W * 0.5;
    const double card_y = frame.y() + H * 0.50;

    const double pw_w = 280.0;
    const double pw_h = 44.0;
    const double pw_x = cx - pw_w * 0.5 + static_cast<double>(m_shake_offset);
    const double pw_y = card_y + 140.0;

    if (m_input) {
        m_input->layout(txui::Rect(pw_x, pw_y, pw_w, pw_h));
    }
}

void LockWidget::paint_override(txui::Painter& painter) const noexcept {
    const double W = frame().width();
    const double H = frame().height();
    const double cx = frame().x() + W * 0.5;

    // ── 1. Cosmic Background Gradient
    painter.fill_gradient_rect(frame(), BG_TOP, BG_BOT);

    // ── 2. Time & Date block (Precise FontMetrics Centering)
    time_t now = time(nullptr);
    struct tm t_buf;
    struct tm* t = (localtime_r(&now, &t_buf) != nullptr) ? &t_buf : nullptr;
    char time_buf[8]  = "??:??";
    char date_buf[48] = "";
    if (t) {
        strftime(time_buf, sizeof(time_buf), "%H:%M", t);
        strftime(date_buf, sizeof(date_buf), "%A, %B %d", t);
    }

    // Large Time Text (Explicit 64px font size with exact FreeType width)
    constexpr double TIME_FONT_SIZE = 64.0;
    const auto time_ext = txui::FontMetrics::measure(time_buf, TIME_FONT_SIZE, true, txui::FontFamily::UI);
    const double time_x = cx - time_ext.width * 0.5;
    const double time_y = frame().y() + H * 0.20;
    painter.draw_text(txui::Point(time_x, time_y), time_buf, TIME_COLOR, TIME_FONT_SIZE, true);

    // Date Subtitle (Explicit 15px font size with exact FreeType width)
    constexpr double DATE_FONT_SIZE = 15.0;
    const auto date_ext = txui::FontMetrics::measure(date_buf, DATE_FONT_SIZE, false, txui::FontFamily::UI);
    const double date_x = cx - date_ext.width * 0.5;
    const double date_y = time_y + time_ext.height + 12.0;
    painter.draw_text(txui::Point(date_x, date_y), date_buf, DATE_COLOR, DATE_FONT_SIZE);

    // ── 3. Central Login Card
    const double card_w = 360.0;
    const double card_h = 240.0;
    const double card_x = cx - card_w * 0.5;
    const double card_y = frame().y() + H * 0.50;
    const txui::Rect card_rect(card_x, card_y, card_w, card_h);

    // Card background & sleek border (Radius 20px / 19px)
    painter.fill_rounded_rect(
        txui::Rect(card_x - 1.0, card_y - 1.0, card_w + 2.0, card_h + 2.0),
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

    // User greeting — Centered via real FontMetrics
    const std::string user_name = "Tinexus User";
    constexpr double USER_FONT_SIZE = 15.0;
    const auto user_ext = txui::FontMetrics::measure(user_name, USER_FONT_SIZE, true, txui::FontFamily::UI);
    const double user_x = cx - user_ext.width * 0.5;
    painter.draw_text(txui::Point(user_x, avatar_y + 38.0), user_name, USER_COLOR, USER_FONT_SIZE, true);

    // ── 5. Password Input (Rendered via child txui::TextInput widget with shake offset)
    if (m_input) {
        const double pw_w = 280.0;
        const double pw_h = 44.0;
        const double pw_x = cx - pw_w * 0.5 + static_cast<double>(m_shake_offset);
        const double pw_y = card_y + 140.0;
        m_input->layout(txui::Rect(pw_x, pw_y, pw_w, pw_h));
        m_input->paint(painter);
    }

    // ── 6. Bottom info bar — Centered via real FontMetrics
    const double bottom_y = frame().y() + H - 32.0;
    const std::string bottom_hint = "Tinexus Platform  •  Horizon v0.1";
    constexpr double HINT_FONT_SIZE = 12.0;
    const auto hint_ext = txui::FontMetrics::measure(bottom_hint, HINT_FONT_SIZE, false, txui::FontFamily::UI);
    const double bottom_x = cx - hint_ext.width * 0.5;
    painter.draw_text(txui::Point(bottom_x, bottom_y), bottom_hint,
                      txui::Color(140, 150, 180, 120), HINT_FONT_SIZE);
}

} // namespace tinexus::lock
