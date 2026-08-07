#include <txui/animation/KineticScroller.hpp>
#include <cmath>
#include <algorithm>

namespace txui {

KineticScroller::KineticScroller() = default;

void KineticScroller::reset(double current_position, double min_position, double max_position) {
    m_position = current_position;
    m_min_pos = min_position;
    m_max_pos = max_position;
    m_velocity = 0.0;
    m_animating = false;
}

void KineticScroller::drag(double delta) {
    m_position += delta;
    m_animating = false;
    
    // Resistance when dragging out of bounds
    if (m_position < m_min_pos) {
        double over = m_min_pos - m_position;
        m_position = m_min_pos - std::log1p(over) * 10.0;
    } else if (m_position > m_max_pos) {
        double over = m_position - m_max_pos;
        m_position = m_max_pos + std::log1p(over) * 10.0;
    }
}

void KineticScroller::release(double velocity) {
    m_velocity = velocity;
    m_animating = true;
}

bool KineticScroller::is_overscrolling() const noexcept {
    return m_position < m_min_pos || m_position > m_max_pos;
}

bool KineticScroller::update(std::chrono::milliseconds delta_time) {
    if (!m_animating) return false;

    double dt = static_cast<double>(delta_time.count()) / 1000.0; // in seconds
    if (dt <= 0.0) return m_animating;

    if (is_overscrolling()) {
        // Apply spring physics to return to bounds
        double target = (m_position < m_min_pos) ? m_min_pos : m_max_pos;
        double displacement = m_position - target;
        
        // F = -kx - cv
        double spring_force = -SPRING_TENSION * displacement;
        double damping_force = -SPRING_FRICTION * m_velocity;
        double acceleration = spring_force + damping_force;
        
        m_velocity += acceleration * dt;
        m_position += m_velocity * dt;

        // Snap to rest if very close to target and moving slowly
        if (std::abs(displacement) < 0.5 && std::abs(m_velocity) < VELOCITY_THRESHOLD) {
            m_position = target;
            m_velocity = 0.0;
            m_animating = false;
        }
    } else {
        // Normal momentum scrolling with deceleration
        m_position += m_velocity * dt;
        m_velocity *= std::pow(DECELERATION_RATE, delta_time.count());

        // Stop if too slow
        if (std::abs(m_velocity) < VELOCITY_THRESHOLD) {
            m_velocity = 0.0;
            m_animating = false;
        }
    }

    return m_animating;
}

} // namespace txui
