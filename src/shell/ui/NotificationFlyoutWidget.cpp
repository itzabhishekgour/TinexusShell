#include "shell/ui/NotificationFlyoutWidget.hpp"
#include <txui/render/FontMetrics.hpp>

namespace tinexus::shell {

namespace {
    constexpr txui::Color CARD_BG       { 22,  24,  32, 248};
    constexpr txui::Color CARD_HOVER    { 44,  48,  62, 255};
    constexpr txui::Color BORDER_LINE   {255, 255, 255,  15};
    constexpr txui::Color TXT_PRI       {245, 245, 250, 255};
    constexpr txui::Color TXT_SEC       {175, 180, 195, 255};
    constexpr txui::Color TXT_DIM       {120, 125, 140, 200};

    std::string truncate_to_width(const std::string& text, double max_w, double font_size) {
        if (text.empty()) return text;
        if (txui::FontMetrics::measure(text, font_size).width <= max_w) {
            return text;
        }
        std::string res = text;
        while (!res.empty() && txui::FontMetrics::measure(res + "...", font_size).width > max_w) {
            res.pop_back();
        }
        return res + "...";
    }
}

NotificationFlyoutWidget::NotificationFlyoutWidget() = default;

double NotificationFlyoutWidget::calculate_height() const noexcept {
    return m_notifications.empty() ? 120.0 : (48.0 + static_cast<double>(m_notifications.size()) * 74.0);
}

txui::Size NotificationFlyoutWidget::measure_override(const txui::Constraints&) noexcept {
    return txui::Size(390.0, calculate_height());
}

void NotificationFlyoutWidget::layout_override(const txui::Rect&) noexcept {
}

void NotificationFlyoutWidget::paint_override(txui::Painter& painter) const noexcept {
    const auto& f = frame();
    const double nx = f.x(), ny = f.y();
    const double nw = f.width(), nh = f.height();

    // 1. Drop shadow & card
    painter.fill_rounded_rect(txui::Rect(nx - 4.0, ny + 6.0, nw + 8.0, nh + 4.0), 14.0, txui::Color(0, 0, 0, 95));
    painter.fill_rounded_rect(txui::Rect(nx - 1.0, ny - 1.0, nw + 2.0, nh + 2.0), 15.0, BORDER_LINE);
    painter.fill_rounded_rect(txui::Rect(nx, ny, nw, nh), 14.0, CARD_BG);

    // 2. Header
    std::string header = "Notifications (" + std::to_string(m_notifications.size()) + ")";
    painter.draw_text(txui::Point(nx + 16.0, ny + 13.0), header, TXT_PRI, 13.5, true);

    if (!m_notifications.empty()) {
        // [Clear All] Pill button
        painter.fill_rounded_rect(txui::Rect(nx + nw - 87.0, ny + 9.0, 74.0, 26.0), 7.0, txui::Color(255, 255, 255, 20));
        painter.fill_rounded_rect(txui::Rect(nx + nw - 86.0, ny + 10.0, 72.0, 24.0), 6.0,
                                  m_hover_clear ? CARD_HOVER : txui::Color(36, 40, 52, 255));
        double clr_len = txui::FontMetrics::measure("Clear All", 11.5).width;
        double clr_x = (nx + nw - 86.0) + (72.0 - clr_len) * 0.5;
        painter.draw_text(txui::Point(clr_x, ny + 14.0), "Clear All", TXT_SEC, 11.5);

        double cur_ny = ny + 44.0;
        for (size_t i = 0; i < m_notifications.size(); ++i) {
            const auto& n = m_notifications[i];
            bool hov = (m_hovered_index == static_cast<int>(i));

            painter.fill_rounded_rect(txui::Rect(nx + 7.0, cur_ny - 1.0, nw - 14.0, 68.0), 9.0, BORDER_LINE);
            painter.fill_rounded_rect(txui::Rect(nx + 8.0, cur_ny, nw - 16.0, 66.0), 8.0,
                                      hov ? CARD_HOVER : txui::Color(30, 32, 42, 220));

            // Urgency dot (centered at y = cur_ny + 33.0)
            painter.fill_circle(txui::Point(nx + 26.0, cur_ny + 33.0), 8.0, n.icon_col);
            painter.fill_circle(txui::Point(nx + 26.0, cur_ny + 33.0), 3.0, txui::Color(255, 255, 255, 240));

            // Title with exact font metrics truncation
            const double max_title_w = nw - 140.0;
            std::string disp_title = truncate_to_width(n.title, max_title_w, 13.0);
            painter.draw_text(txui::Point(nx + 44.0, cur_ny + 12.0), disp_title, TXT_PRI, 13.0, true);

            double time_w = txui::FontMetrics::measure(n.time_ago, 11.0).width;
            painter.draw_text(txui::Point(nx + nw - time_w - 18.0, cur_ny + 12.0), n.time_ago, TXT_DIM, 11.0);

            // Body with exact font metrics truncation
            const double max_body_w = nw - 62.0;
            std::string disp_body = truncate_to_width(n.body, max_body_w, 11.5);
            painter.draw_text(txui::Point(nx + 44.0, cur_ny + 34.0), disp_body, TXT_SEC, 11.5);

            cur_ny += 74.0;
        }
    } else {
        painter.draw_circle(txui::Point(nx + nw * 0.5, ny + 58.0), 16.0, 1.5, TXT_DIM);
        double empty_w = txui::FontMetrics::measure("No New Notifications", 12.5).width;
        painter.draw_text(txui::Point(nx + (nw - empty_w) * 0.5, ny + 86.0), "No New Notifications", TXT_DIM, 12.5);
    }
}

bool NotificationFlyoutWidget::handle_event(const txui::Event& event) noexcept {
    const auto& f = frame();
    const double nx = f.x(), ny = f.y();
    const double nw = f.width();

    if (event.type == txui::EventType::PointerMove) {
        double mx = event.pointer.x, my = event.pointer.y;

        bool clr = (mx >= nx + nw - 86.0 && mx <= nx + nw - 14.0 && my >= ny + 10.0 && my <= ny + 34.0);
        int n_hov = -1;
        if (mx >= nx + 8.0 && mx <= nx + nw - 8.0 && my >= ny + 44.0) {
            n_hov = static_cast<int>((my - (ny + 44.0)) / 74.0);
            if (n_hov >= static_cast<int>(m_notifications.size())) n_hov = -1;
        }

        if (m_hover_clear != clr || m_hovered_index != n_hov) {
            m_hover_clear = clr;
            m_hovered_index = n_hov;
            mark_needs_paint();
        }
        return f.contains(txui::Point(mx, my));
    }

    if (event.type == txui::EventType::PointerButtonPress) {
        if (event.pointer.button == txui::MouseButton::Left) {
            double mx = event.pointer.x, my = event.pointer.y;
            if (mx >= nx + nw - 86.0 && mx <= nx + nw - 14.0 && my >= ny + 10.0 && my <= ny + 34.0) {
                clear_notifications();
                if (on_clear_all) on_clear_all();
                return true;
            }
        }
    }

    return false;
}

} // namespace tinexus::shell
