#include "notifications/NotificationBubble.hpp"
#include <algorithm>
#include <cmath>

namespace tinexus::notifications {

NotificationBubble::NotificationBubble(const NotificationItem& item)
    : m_item(item)
{
    m_state = BubbleState::SlidingIn;
    m_offset_x = 300.0;
    m_opacity = 0.0;
    m_display_time_ms = 0.0;
}

bool NotificationBubble::update(std::chrono::milliseconds delta_time) {
    if (m_state == BubbleState::Hidden) return false;

    double dt = static_cast<double>(delta_time.count()) / 1000.0;

    switch (m_state) {
        case BubbleState::SlidingIn:
            m_offset_x -= SLIDE_SPEED * dt;
            m_opacity += 4.0 * dt;
            if (m_offset_x <= 0.0) {
                m_offset_x = 0.0;
                m_opacity = 1.0;
                m_state = BubbleState::Visible;
            }
            break;

        case BubbleState::Visible:
            m_display_time_ms += delta_time.count();
            if (m_display_time_ms >= TIMEOUT_MS) {
                dismiss();
            }
            break;

        case BubbleState::SlidingOut:
            m_offset_x += SLIDE_SPEED * dt;
            m_opacity -= 4.0 * dt;
            if (m_offset_x >= 300.0 || m_opacity <= 0.0) {
                m_offset_x = 300.0;
                m_opacity = 0.0;
                m_state = BubbleState::Hidden;
            }
            break;
            
        case BubbleState::Hidden:
            break;
    }

    m_opacity = std::clamp(m_opacity, 0.0, 1.0);
    return true;
}

void NotificationBubble::handle_drag(double delta_x) {
    if (m_state != BubbleState::Visible) return;
    
    // Allow dragging to the right (dismiss) but resist dragging to the left
    if (m_offset_x + delta_x < 0) {
        m_offset_x += delta_x * 0.1; // Rubber-banding resistance
    } else {
        m_offset_x += delta_x;
    }
}

void NotificationBubble::handle_release(double velocity_x) {
    if (m_state != BubbleState::Visible) return;

    if (m_offset_x > SWIPE_THRESHOLD || velocity_x > 500.0) {
        dismiss(); // Swipe successful
    } else {
        m_state = BubbleState::SlidingIn; // Snap back
    }
}

void NotificationBubble::dismiss() {
    m_state = BubbleState::SlidingOut;
}

} // namespace tinexus::notifications
