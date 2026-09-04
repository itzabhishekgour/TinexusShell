#ifndef TINEXUS_NOTIFICATION_BUBBLE_HPP
#define TINEXUS_NOTIFICATION_BUBBLE_HPP

#include "notifications/notification_item.hpp"
#include <txui/render/Painter.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Point.hpp>
#include <chrono>
#include <string>
#include <vector>

namespace tinexus::notifications {

enum class BubbleState {
    Hidden,
    SlidingIn,
    Visible,
    SlidingOut
};

class NotificationBubble {
public:
    explicit NotificationBubble(const NotificationItem& item);

    // Updates physics/timer. Returns true while active, false when completely hidden.
    bool update(std::chrono::milliseconds delta_time);

    void handle_drag(double delta_x);
    void handle_release(double velocity_x);
    void dismiss();

    // Render this bubble at vertical position `y`
    void paint(txui::Painter& painter, double y, double width, double pointer_x, double pointer_y) const noexcept;

    // Hit testing
    [[nodiscard]] bool hit_test_close(double local_x, double local_y, double width) const noexcept;
    [[nodiscard]] int hit_test_action(double local_x, double local_y, double width) const noexcept;

    [[nodiscard]] double height() const noexcept;
    [[nodiscard]] double offset_x() const noexcept { return m_offset_x; }
    [[nodiscard]] double opacity() const noexcept { return m_opacity; }
    [[nodiscard]] BubbleState state() const noexcept { return m_state; }
    [[nodiscard]] const NotificationItem& item() const noexcept { return m_item; }
    [[nodiscard]] NotificationId id() const noexcept { return m_item.id; }

private:
    NotificationItem m_item;
    BubbleState m_state{BubbleState::SlidingIn};

    double m_offset_x{420.0};
    double m_opacity{0.0};
    double m_display_time_ms{0.0};
    double m_total_time_ms{6000.0};

    static constexpr double SLIDE_SPEED = 1400.0;
    static constexpr double SWIPE_THRESHOLD = 150.0;
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATION_BUBBLE_HPP
