#ifndef TXUI_KINETIC_SCROLLER_HPP
#define TXUI_KINETIC_SCROLLER_HPP

#include <chrono>

namespace txui {

// Simulates natural scrolling physics (momentum, deceleration, and rubber-banding).
// Based on typical spring physics used in iOS/macOS.
class KineticScroller {
public:
    KineticScroller();

    // Reset the scroller with a new layout boundary and current position.
    void reset(double current_position, double min_position, double max_position);

    // Call this when the user is actively dragging the scroll view.
    void drag(double delta);

    // Call this when the user releases the scroll view to impart initial velocity.
    void release(double velocity);

    // Update the simulation. Returns true if the position changed and animation is active.
    bool update(std::chrono::milliseconds delta_time);

    // Get the current simulated scroll position.
    [[nodiscard]] double position() const noexcept { return m_position; }

    // Returns true if the content is currently out of bounds (rubber-banding).
    [[nodiscard]] bool is_overscrolling() const noexcept;

    // Returns true if the scroller is actively animating (not at rest).
    [[nodiscard]] bool is_animating() const noexcept { return m_animating; }

private:
    double m_position{0.0};
    double m_velocity{0.0};
    double m_min_pos{0.0};
    double m_max_pos{0.0};
    bool m_animating{false};

    // Physics parameters
    static constexpr double DECELERATION_RATE = 0.998; // Friction
    static constexpr double SPRING_TENSION = 150.0;    // How hard the edge pulls back
    static constexpr double SPRING_FRICTION = 15.0;    // Damping of the bounce
    static constexpr double VELOCITY_THRESHOLD = 0.1;  // Below this, we snap to rest
};

} // namespace txui

#endif // TXUI_KINETIC_SCROLLER_HPP
