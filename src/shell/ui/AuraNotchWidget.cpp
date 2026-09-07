#include "shell/ui/AuraNotchWidget.hpp"
#include <txui/render/FontMetrics.hpp>
#include <ctime>
#include <cstring>
#include <cmath>

namespace tinexus::shell {

namespace {
    constexpr txui::Color NOTCH_BG      { 36,  40,  54, 245};
    constexpr txui::Color NOTCH_RIM     {255, 255, 255,  35};
    constexpr txui::Color TIME_PILL_BG  { 20,  22,  30, 220};
    constexpr txui::Color TIME_PILL_RIM {255, 255, 255,  22};
    constexpr txui::Color TXT_PRI       {245, 245, 250, 255};
    constexpr txui::Color ACCENT_BLUE   { 59, 130, 246, 255};
    constexpr txui::Color ACCENT_CYAN   { 56, 189, 248, 255};
}

AuraNotchWidget::AuraNotchWidget() = default;

txui::Size AuraNotchWidget::measure_override(const txui::Constraints& constraints) noexcept {
    double w = constraints.max_width;
    if (w <= 0.0) w = 300.0;
    return txui::Size(w, 46.0);
}

void AuraNotchWidget::layout_override(const txui::Rect&) noexcept {
}

void AuraNotchWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto& f = frame();
    const double cx = f.x() + f.width() * 0.5;

    // Center: Aura v2 Sloped Trapezoid Notch (\______/) (46px high, hangs down below 32px bar)
    constexpr double NOTCH_H = 46.0;
    constexpr double NOTCH_TOP_HALF = 136.0; // from cx - 136 to cx + 136
    constexpr double NOTCH_BOT_HALF = 98.0;  // from cx - 98 to cx + 98
    constexpr double NOTCH_SLOPE = NOTCH_TOP_HALF - NOTCH_BOT_HALF; // 38.0

    for (int y = 0; y <= static_cast<int>(NOTCH_H); ++y) {
        double t = static_cast<double>(y) / NOTCH_H;
        double lx = (cx - NOTCH_TOP_HALF) + t * NOTCH_SLOPE;
        double rx = (cx + NOTCH_TOP_HALF) - t * NOTCH_SLOPE;
        painter.fill_rect(txui::Rect(lx, f.y() + static_cast<double>(y), rx - lx, 1.0), NOTCH_BG);
    }

    // Notch rim outline
    painter.draw_line(txui::Point(cx - NOTCH_TOP_HALF, f.y()), txui::Point(cx - NOTCH_BOT_HALF, f.y() + NOTCH_H), 1.2, NOTCH_RIM);
    painter.draw_line(txui::Point(cx - NOTCH_BOT_HALF, f.y() + NOTCH_H), txui::Point(cx + NOTCH_BOT_HALF, f.y() + NOTCH_H), 1.2, NOTCH_RIM);
    painter.draw_line(txui::Point(cx + NOTCH_BOT_HALF, f.y() + NOTCH_H), txui::Point(cx + NOTCH_TOP_HALF, f.y()), 1.2, NOTCH_RIM);

    // Live Clock & Date Assembly
    time_t raw_now = time(nullptr);
    struct tm tb;
    localtime_r(&raw_now, &tb);

    char ts[32];
    strftime(ts, sizeof(ts), "%I:%M %p", &tb);
    char* time_disp = (ts[0] == '0') ? ts + 1 : ts;

    // Time pill capsule on the Left: (cx - 96.0 to cx - 24.0, w=72.0)
    painter.fill_rounded_rect(txui::Rect(cx - 96.0, f.y() + 11.0, 72.0, 24.0), 12.0, TIME_PILL_BG);
    painter.fill_rounded_rect(txui::Rect(cx - 96.0, f.y() + 11.0, 72.0, 24.0), 12.0, TIME_PILL_RIM);
    double time_w = txui::FontMetrics::measure(time_disp, 12.0).width;
    double time_x = (cx - 96.0) + (72.0 - time_w) * 0.5;
    painter.draw_text(txui::Point(time_x, f.y() + 15.0), time_disp, TXT_PRI, 12.0, true);

    // Center Avatar Circle 'T' (Exact Center cx, y = 23.0)
    double cy = f.y() + 23.0;
    if (m_hover_center) {
        painter.draw_glow(txui::Point(cx, cy), 16.0, 24.0, txui::Color(56, 189, 248, 120));
        painter.fill_circle(txui::Point(cx, cy), 15.0, txui::Color(56, 189, 248, 255));
    } else {
        painter.draw_glow(txui::Point(cx, cy), 14.0, 22.0, txui::Color(59, 130, 246, 75));
        painter.fill_circle(txui::Point(cx, cy), 14.0, ACCENT_BLUE);
    }
    painter.draw_circle(txui::Point(cx, cy), 14.5, 1.2, txui::Color(147, 197, 253, 180));
    painter.draw_text(txui::Point(cx - 5.0, f.y() + 15.0), "T", txui::Color(255, 255, 255, 255), 14.0, true);

    // Date pill capsule on the Right: (cx + 24.0 to cx + 96.0, w=72.0)
    int cur_day = tb.tm_mday;
    const char* sfx = "th";
    if (cur_day % 10 == 1 && cur_day != 11) sfx = "st";
    else if (cur_day % 10 == 2 && cur_day != 12) sfx = "nd";
    else if (cur_day % 10 == 3 && cur_day != 13) sfx = "rd";
    char month_abbr[16];
    strftime(month_abbr, sizeof(month_abbr), "%b", &tb);
    char ds[32];
    snprintf(ds, sizeof(ds), "%s %d%s", month_abbr, cur_day, sfx);

    constexpr double DATE_PILL_W = 72.0;
    if (m_hover_date || m_calendar_open) {
        painter.fill_rounded_rect(txui::Rect(cx + 24.0, f.y() + 11.0, DATE_PILL_W, 24.0), 12.0, txui::Color(255, 255, 255, 30));
    } else {
        painter.fill_rounded_rect(txui::Rect(cx + 24.0, f.y() + 11.0, DATE_PILL_W, 24.0), 12.0, TIME_PILL_BG);
    }
    painter.fill_rounded_rect(txui::Rect(cx + 24.0, f.y() + 11.0, DATE_PILL_W, 24.0), 12.0, TIME_PILL_RIM);
    double date_w = txui::FontMetrics::measure(ds, 12.0).width;
    double date_x = (cx + 24.0) + (DATE_PILL_W - date_w) * 0.5;
    painter.draw_text(txui::Point(date_x, f.y() + 15.0), ds, (m_hover_date || m_calendar_open) ? ACCENT_CYAN : TXT_PRI, 12.0, true);
}

bool AuraNotchWidget::handle_event(const txui::Event& event) noexcept {
    const auto& f = frame();
    const double cx = f.x() + f.width() * 0.5;

    if (event.type == txui::EventType::PointerMove) {
        double mx = event.pointer.x, my = event.pointer.y;
        bool h_cen = (std::hypot(mx - cx, my - (f.y() + 23.0)) <= 16.0);
        bool h_dat = (mx >= cx + 24.0 && mx <= cx + 96.0 && my >= f.y() + 6.0 && my <= f.y() + 38.0);

        if (m_hover_center != h_cen || m_hover_date != h_dat) {
            m_hover_center = h_cen;
            m_hover_date = h_dat;
            mark_needs_paint();
        }
        return h_cen || h_dat;
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            double mx = event.pointer.x, my = event.pointer.y;
            if (std::hypot(mx - cx, my - (f.y() + 23.0)) <= 16.0) {
                if (on_center_clicked) on_center_clicked();
                return true;
            }
            if (mx >= cx + 24.0 && mx <= cx + 96.0 && my >= f.y() + 6.0 && my <= f.y() + 38.0) {
                if (on_date_clicked) on_date_clicked();
                return true;
            }
        }
    }

    return false;
}

} // namespace tinexus::shell
