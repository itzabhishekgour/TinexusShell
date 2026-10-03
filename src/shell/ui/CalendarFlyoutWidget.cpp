#include "shell/ui/CalendarFlyoutWidget.hpp"
#include <txui/render/FontMetrics.hpp>
#include <ctime>
#include <string>

namespace tinexus::shell {

namespace {
    constexpr txui::Color CARD_BG       { 22,  24,  32, 248};
    constexpr txui::Color BORDER_LINE   {255, 255, 255,  15};
    constexpr txui::Color TXT_PRI       {245, 245, 250, 255};
    constexpr txui::Color TXT_SEC       {175, 180, 195, 255};
    constexpr txui::Color TXT_DIM       {120, 125, 140, 200};
    constexpr txui::Color ACCENT_BLUE   { 59, 130, 246, 255};
    constexpr txui::Color ACCENT_CYAN   { 56, 189, 248, 255};

    int days_in_month(int year, int month) {
        if (month == 2) {
            bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
            return leap ? 29 : 28;
        }
        if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
        return 31;
    }

    int day_of_week(int year, int month, int day) {
        struct tm t = {};
        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        mktime(&t);
        return t.tm_wday;
    }
}

CalendarFlyoutWidget::CalendarFlyoutWidget() = default;

txui::Size CalendarFlyoutWidget::measure_override(const txui::Constraints&) noexcept {
    return txui::Size(300.0, 285.0);
}

void CalendarFlyoutWidget::layout_override(const txui::Rect&) noexcept {
}

void CalendarFlyoutWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto& f = frame();
    const double cal_x = f.x();
    const double cal_y = f.y();
    const double cw = f.width();
    const double ch = f.height();

    // 1. Drop shadow & card
    painter.fill_rounded_rect(txui::Rect(cal_x - 4.0, cal_y + 8.0, cw + 8.0, ch + 4.0), 16.0, txui::Color(0, 0, 0, 100));
    painter.fill_rounded_rect(txui::Rect(cal_x, cal_y, cw, ch), 16.0, CARD_BG);
    painter.fill_rounded_rect(txui::Rect(cal_x - 1.0, cal_y - 1.0, cw + 2.0, ch + 2.0), 17.0, BORDER_LINE);

    time_t raw_now = time(nullptr);
    struct tm tb;
    localtime_r(&raw_now, &tb);

    int cur_year = tb.tm_year + 1900;
    int cur_month = tb.tm_mon + 1 + m_nav_offset;
    while (cur_month > 12) { cur_month -= 12; cur_year++; }
    while (cur_month < 1)  { cur_month += 12; cur_year--; }

    const char* month_names[] = {"January","February","March","April","May","June",
                                 "July","August","September","October","November","December"};
    std::string header_title = std::string(month_names[cur_month - 1]) + " " + std::to_string(cur_year);

    // Left navigation chevron
    txui::Color prev_col = m_hover_prev ? txui::Color(255, 255, 255, 255) : ACCENT_CYAN;
    painter.draw_line(txui::Point(cal_x + 22.0, cal_y + 14.0), txui::Point(cal_x + 16.0, cal_y + 18.5), 1.8, prev_col);
    painter.draw_line(txui::Point(cal_x + 16.0, cal_y + 18.5), txui::Point(cal_x + 22.0, cal_y + 23.0), 1.8, prev_col);

    // Header title centered via FontMetrics
    double title_w = txui::FontMetrics::measure(header_title, 14.0).width;
    double h_offset = (cw - title_w) * 0.5;
    painter.draw_text(txui::Point(cal_x + h_offset, cal_y + 12.0), header_title, TXT_PRI, 14.0, true);

    // Right navigation chevron
    txui::Color next_col = m_hover_next ? txui::Color(255, 255, 255, 255) : ACCENT_CYAN;
    painter.draw_line(txui::Point(cal_x + cw - 22.0, cal_y + 14.0), txui::Point(cal_x + cw - 16.0, cal_y + 18.5), 1.8, next_col);
    painter.draw_line(txui::Point(cal_x + cw - 16.0, cal_y + 18.5), txui::Point(cal_x + cw - 22.0, cal_y + 23.0), 1.8, next_col);

    // Day of week headers
    const char* dows[] = { "SU", "MO", "TU", "WE", "TH", "FR", "SA" };
    double col_w = (cw - 24.0) / 7.0;
    for (int d = 0; d < 7; ++d) {
        double dow_w = txui::FontMetrics::measure(dows[d], 11.0).width;
        double dow_x = cal_x + 12.0 + static_cast<double>(d) * col_w + (col_w - dow_w) * 0.5;
        painter.draw_text(txui::Point(dow_x, cal_y + 42.0), dows[d], TXT_DIM, 11.0, true);
    }
    painter.draw_line(txui::Point(cal_x + 14.0, cal_y + 58.0), txui::Point(cal_x + cw - 14.0, cal_y + 58.0), 1.0, BORDER_LINE);

    int start_dow = day_of_week(cur_year, cur_month, 1);
    int total_days = days_in_month(cur_year, cur_month);

    int today_day = (m_nav_offset == 0) ? tb.tm_mday : -1;
    double row_y = cal_y + 66.0;
    int current_col = start_dow;

    for (int day = 1; day <= total_days; ++day) {
        double day_cx = cal_x + 12.0 + static_cast<double>(current_col) * col_w + col_w * 0.5;
        double day_cy = row_y + 11.0;

        bool is_today = (day == today_day);
        bool is_hover = (m_hovered_day == day);

        if (is_today) {
            painter.fill_circle(txui::Point(day_cx, day_cy), 12.0, ACCENT_BLUE);
        } else if (is_hover) {
            painter.fill_circle(txui::Point(day_cx, day_cy), 12.0, txui::Color(255, 255, 255, 25));
        }

        std::string day_str = std::to_string(day);
        double str_w = txui::FontMetrics::measure(day_str, 12.0).width;
        painter.draw_text(txui::Point(day_cx - str_w * 0.5, day_cy - 6.0), day_str,
                          is_today ? txui::Color(255, 255, 255, 255) : (is_hover ? TXT_PRI : TXT_SEC),
                          12.0, is_today);

        current_col++;
        if (current_col > 6) {
            current_col = 0;
            row_y += 26.0;
        }
    }

    // Live clock footer
    painter.draw_line(txui::Point(cal_x + 14.0, cal_y + ch - 32.0), txui::Point(cal_x + cw - 14.0, cal_y + ch - 32.0), 1.0, BORDER_LINE);
    char full_time[32];
    strftime(full_time, sizeof(full_time), "Live Clock: %I:%M:%S %p", &tb);
    painter.draw_text(txui::Point(cal_x + 20.0, cal_y + ch - 22.0), full_time, ACCENT_CYAN, 11.5);
}

bool CalendarFlyoutWidget::handle_event(const txui::Event& event) noexcept {
    const auto& f = frame();
    const double cal_x = f.x();
    const double cal_y = f.y();
    const double cw = f.width();

    if (event.type == txui::EventType::PointerMove) {
        double mx = event.pointer.x, my = event.pointer.y;

        bool h_prev = (mx >= cal_x + 10.0 && mx <= cal_x + 35.0 && my >= cal_y + 8.0 && my <= cal_y + 32.0);
        bool h_next = (mx >= cal_x + cw - 35.0 && mx <= cal_x + cw - 10.0 && my >= cal_y + 8.0 && my <= cal_y + 32.0);

        int h_day = -1;
        if (my >= cal_y + 60.0 && my <= cal_y + 240.0 && mx >= cal_x + 12.0 && mx <= cal_x + cw - 12.0) {
            double col_w = (cw - 24.0) / 7.0;
            int col = static_cast<int>((mx - (cal_x + 12.0)) / col_w);
            int row = static_cast<int>((my - (cal_y + 66.0)) / 26.0);

            time_t raw_now = time(nullptr);
            struct tm tb;
            localtime_r(&raw_now, &tb);
            int cur_year = tb.tm_year + 1900;
            int cur_month = tb.tm_mon + 1 + m_nav_offset;
            while (cur_month > 12) { cur_month -= 12; cur_year++; }
            while (cur_month < 1)  { cur_month += 12; cur_year--; }

            int start_dow = day_of_week(cur_year, cur_month, 1);
            int total_days = days_in_month(cur_year, cur_month);

            int day = row * 7 + col - start_dow + 1;
            if (day >= 1 && day <= total_days) {
                h_day = day;
            }
        }

        if (m_hover_prev != h_prev || m_hover_next != h_next || m_hovered_day != h_day) {
            m_hover_prev = h_prev;
            m_hover_next = h_next;
            m_hovered_day = h_day;
            mark_needs_paint();
        }
        return f.contains(txui::Point(mx, my));
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            double mx = event.pointer.x, my = event.pointer.y;
            if (mx >= cal_x + 10.0 && mx <= cal_x + 35.0 && my >= cal_y + 8.0 && my <= cal_y + 32.0) {
                m_nav_offset--;
                mark_needs_paint();
                return true;
            }
            if (mx >= cal_x + cw - 35.0 && mx <= cal_x + cw - 10.0 && my >= cal_y + 8.0 && my <= cal_y + 32.0) {
                m_nav_offset++;
                mark_needs_paint();
                return true;
            }
        }
    }

    return false;
}

} // namespace tinexus::shell
