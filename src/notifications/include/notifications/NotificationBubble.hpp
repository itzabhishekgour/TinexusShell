#ifndef TINEXUS_NOTIFICATION_BUBBLE_HPP
#define TINEXUS_NOTIFICATION_BUBBLE_HPP

#include "notifications/notification_item.hpp"
#include <chrono>

namespace tinexus::notifications {

// Represents the animation state of a notification bubble
enum class BubbleState {
    Hidden,
    SlidingIn,
    Visible,
    SlidingOut
};

// Manages the UI lifecycle and physics-based animation of a single notification bubble.
// Integrates with txui::KineticScroller concepts for swipe-to-dismiss behavior.
class NotificationBubble {
public:
    explicit NotificationBubble(const NotificationItem& item);

    // Update the animation state based on delta time
    // Returns false if the bubble is Hidden and can be destroyed
    bool update(std::chrono::milliseconds delta_time);

    // Handle user input (e.g. mouse drag for swipe-to-dismiss)
    void handle_drag(double delta_x);
    void handle_release(double velocity_x);

    // Trigger the slide-out animation (e.g. timeout reached or dismissed)
    void dismiss();

    // Get current render properties
    [[nodiscard]] double offset_x() const noexcept { return m_offset_x; }
    [[nodiscard]] double opacity() const noexcept { return m_opacity; }
    [[nodiscard]] BubbleState state() const noexcept { return m_state; }
    [[nodiscard]] const NotificationItem& item() const noexcept { return m_item; }

private:
    NotificationItem m_item;
    BubbleState m_state{BubbleState::Hidden};
    
    double m_offset_x{300.0}; // Start off-screen to the right
    double m_opacity{0.0};
    double m_display_time_ms{0.0};

    // Constants for animation
    static constexpr double SLIDE_SPEED = 1200.0; // pixels per second
    static constexpr double TIMEOUT_MS = 5000.0;  // 5 seconds visible
    static constexpr double SWIPE_THRESHOLD = 150.0;
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATION_BUBBLE_HPP
