#include "notifications/NotificationBubble.hpp"
#include <txui/graphics/Color.hpp>
#include <algorithm>
#include <cmath>

namespace tinexus::notifications {

NotificationBubble::NotificationBubble(const NotificationItem& item)
    : m_item(item)
{
    m_state = BubbleState::SlidingIn;
    m_offset_x = 420.0;
    m_opacity = 0.0;
    m_display_time_ms = 0.0;

    if (item.expire_timeout_ms > 0) {
        m_total_time_ms = static_cast<double>(item.expire_timeout_ms);
    } else if (item.expire_timeout_ms == 0) {
        // Never expire automatically (e.g. persistent modal or critical alert)
        m_total_time_ms = 3600000.0;
    } else {
        // Default based on urgency
        if (item.urgency == Urgency::Critical) {
            m_total_time_ms = 12000.0;
        } else if (item.urgency == Urgency::Low) {
            m_total_time_ms = 4000.0;
        } else {
            m_total_time_ms = 6000.0;
        }
    }
}

double NotificationBubble::height() const noexcept {
    double base_h = 86.0;
    if (!m_item.body.empty() && m_item.body.size() > 40) {
        base_h += 16.0;
    }
    if (!m_item.actions.empty()) {
        base_h += 32.0;
    }
    return base_h;
}

bool NotificationBubble::update(std::chrono::milliseconds delta_time) {
    if (m_state == BubbleState::Hidden) return false;

    double dt = static_cast<double>(delta_time.count()) / 1000.0;

    switch (m_state) {
        case BubbleState::SlidingIn:
            m_offset_x -= SLIDE_SPEED * dt;
            m_opacity += 6.0 * dt;
            if (m_offset_x <= 0.0) {
                m_offset_x = 0.0;
                m_opacity = 1.0;
                m_state = BubbleState::Visible;
            }
            break;

        case BubbleState::Visible:
            m_display_time_ms += static_cast<double>(delta_time.count());
            if (m_item.expire_timeout_ms != 0 && m_display_time_ms >= m_total_time_ms) {
                dismiss();
            }
            break;

        case BubbleState::SlidingOut:
            m_offset_x += SLIDE_SPEED * dt;
            m_opacity -= 5.0 * dt;
            if (m_offset_x >= 420.0 || m_opacity <= 0.0) {
                m_offset_x = 420.0;
                m_opacity = 0.0;
                m_state = BubbleState::Hidden;
            }
            break;

        case BubbleState::Hidden:
            break;
    }

    m_opacity = std::clamp(m_opacity, 0.0, 1.0);
    return m_state != BubbleState::Hidden;
}

void NotificationBubble::handle_drag(double delta_x) {
    if (m_state != BubbleState::Visible) return;

    if (m_offset_x + delta_x < 0) {
        m_offset_x += delta_x * 0.1;
    } else {
        m_offset_x += delta_x;
    }
}

void NotificationBubble::handle_release(double velocity_x) {
    if (m_state != BubbleState::Visible) return;

    if (m_offset_x > SWIPE_THRESHOLD || velocity_x > 500.0) {
        dismiss();
    } else {
        m_state = BubbleState::SlidingIn;
    }
}

void NotificationBubble::dismiss() {
    if (m_state != BubbleState::Hidden && m_state != BubbleState::SlidingOut) {
        m_state = BubbleState::SlidingOut;
    }
}

bool NotificationBubble::hit_test_close(double local_x, double local_y, double width) const noexcept {
    const double h = height();
    if (local_y < 0 || local_y > h) return false;
    // Close button is in top-right: (width - 32, 8, 24, 24)
    return (local_x >= width - 36.0 && local_x <= width - 8.0 && local_y >= 6.0 && local_y <= 34.0);
}

int NotificationBubble::hit_test_action(double local_x, double local_y, double width) const noexcept {
    if (m_item.actions.empty()) return -1;
    const double h = height();
    const double act_y = h - 36.0;
    if (local_y < act_y || local_y > act_y + 26.0) return -1;

    double btn_x = 22.0;
    for (size_t i = 0; i < m_item.actions.size(); ++i) {
        double btn_w = std::max(60.0, static_cast<double>(m_item.actions[i].label.size()) * 8.0 + 20.0);
        if (local_x >= btn_x && local_x <= btn_x + btn_w) {
            return static_cast<int>(i);
        }
        btn_x += btn_w + 8.0;
    }
    return -1;
}

void NotificationBubble::paint(txui::Painter& painter, double y, double width, double pointer_x, double pointer_y) const noexcept {
    if (m_state == BubbleState::Hidden || m_opacity <= 0.001) return;

    const double h = height();
    const double x = m_offset_x;
    const double card_w = width - 8.0;

    // Determine urgency color
    txui::Color stripe_col;
    switch (m_item.urgency) {
        case Urgency::Critical:
            stripe_col = txui::Color(239, 68, 68, static_cast<uint8_t>(255 * m_opacity));
            break;
        case Urgency::Low:
            stripe_col = txui::Color(56, 189, 248, static_cast<uint8_t>(255 * m_opacity));
            break;
        case Urgency::Normal:
        default:
            stripe_col = txui::Color(59, 130, 246, static_cast<uint8_t>(255 * m_opacity));
            break;
    }

    const uint8_t alpha_bg = static_cast<uint8_t>(240 * m_opacity);
    const uint8_t alpha_shadow = static_cast<uint8_t>(160 * m_opacity);
    const uint8_t alpha_text_pri = static_cast<uint8_t>(255 * m_opacity);
    const uint8_t alpha_text_sec = static_cast<uint8_t>(180 * m_opacity);
    const uint8_t alpha_text_dim = static_cast<uint8_t>(130 * m_opacity);

    // 1. Drop shadow
    painter.fill_rounded_rect(
        txui::Rect(x + 2, y + 4, card_w - 4, h),
        16.0,
        txui::Color(0, 0, 0, alpha_shadow)
    );

    // 2. Main card background (Dark glassmorphism gradient)
    painter.fill_gradient_rounded_rect(
        txui::Rect(x + 4, y, card_w - 8, h),
        14.0,
        txui::Color(24, 28, 44, alpha_bg),
        txui::Color(14, 16, 28, alpha_bg)
    );

    // 3. Top border highlight
    painter.fill_rounded_rect(
        txui::Rect(x + 6, y + 1, card_w - 12, 1),
        0.5,
        txui::Color(255, 255, 255, static_cast<uint8_t>(35 * m_opacity))
    );

    // 4. Urgency left accent stripe
    painter.fill_rounded_rect(
        txui::Rect(x + 8, y + 8, 4, h - 16),
        2.0,
        stripe_col
    );

    // If critical, draw subtle glow behind stripe
    if (m_item.urgency == Urgency::Critical) {
        painter.draw_glow(txui::Point(x + 10, y + h * 0.5), 6.0, 16.0, stripe_col);
    }

    // 5. App Icon Badge + Header (App Name + Time)
    const std::string app_display = m_item.app_name.empty() ? "System" : m_item.app_name;
    // App icon mini badge
    painter.fill_rounded_rect(txui::Rect(x + 20, y + 12, 14, 14), 3.0, stripe_col);
    painter.draw_text(txui::Point(x + 40, y + 12), app_display, txui::Color(148, 163, 184, alpha_text_sec), 0.9, true);
    painter.draw_text(txui::Point(x + card_w - 56, y + 12), "now", txui::Color(100, 116, 139, alpha_text_dim), 0.8);

    // Close button (X) top right
    bool close_hover = (pointer_x >= x + card_w - 32.0 && pointer_x <= x + card_w - 12.0 &&
                        pointer_y >= y + 8.0 && pointer_y <= y + 28.0);
    if (close_hover) {
        painter.fill_circle(txui::Point(x + card_w - 22.0, y + 18.0), 9.0, txui::Color(255, 255, 255, 30));
    }
    painter.draw_text(txui::Point(x + card_w - 26.0, y + 11.0), "x",
                      close_hover ? txui::Color::white() : txui::Color(148, 163, 184, alpha_text_dim), 1.0);

    // 6. Summary (Title)
    painter.draw_text(txui::Point(x + 20, y + 32), m_item.summary, txui::Color(255, 255, 255, alpha_text_pri), 1.05, true);

    // 7. Body text
    if (!m_item.body.empty()) {
        std::string body_first = m_item.body;
        std::string body_second = "";
        if (body_first.size() > 42) {
            size_t split_pos = body_first.rfind(' ', 42);
            if (split_pos != std::string::npos && split_pos > 20) {
                body_second = body_first.substr(split_pos + 1);
                body_first = body_first.substr(0, split_pos);
            }
        }
        painter.draw_text(txui::Point(x + 20, y + 54), body_first, txui::Color(203, 213, 225, alpha_text_sec), 0.95);
        if (!body_second.empty()) {
            painter.draw_text(txui::Point(x + 20, y + 70), body_second, txui::Color(203, 213, 225, alpha_text_sec), 0.95);
        }
    }

    // 8. Action Buttons (if any)
    if (!m_item.actions.empty()) {
        const double act_y = y + h - 34.0;
        double btn_x = x + 20.0;
        for (size_t i = 0; i < m_item.actions.size(); ++i) {
            const auto& act = m_item.actions[i];
            double btn_w = std::max(60.0, static_cast<double>(act.label.size()) * 8.0 + 20.0);
            bool act_hover = (pointer_x >= btn_x && pointer_x <= btn_x + btn_w &&
                              pointer_y >= act_y && pointer_y <= act_y + 24.0);

            painter.fill_gradient_rounded_rect(
                txui::Rect(btn_x, act_y, btn_w, 24.0), 6.0,
                act_hover ? txui::Color(59, 130, 246, 200) : txui::Color(40, 48, 70, 200),
                act_hover ? txui::Color(37, 99, 235, 180) : txui::Color(26, 32, 50, 180)
            );
            painter.draw_text(txui::Point(btn_x + 10.0, act_y + 5.0), act.label, txui::Color::white(), 0.9, true);
            btn_x += btn_w + 8.0;
        }
    }

    // 9. Timeout progress line at bottom of card
    if (m_item.expire_timeout_ms != 0 && m_total_time_ms > 0) {
        double progress = std::clamp(1.0 - (m_display_time_ms / m_total_time_ms), 0.0, 1.0);
        double bar_w = (card_w - 16.0) * progress;
        if (bar_w > 0) {
            painter.fill_rounded_rect(
                txui::Rect(x + 8, y + h - 3, bar_w, 2),
                1.0,
                stripe_col
            );
        }
    }
}

} // namespace tinexus::notifications
