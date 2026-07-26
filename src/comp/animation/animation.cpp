#include "comp/animation/animation.hpp"
#include <cmath>

namespace tinexus::comp {

BaseAnimation::BaseAnimation(std::chrono::milliseconds duration, AnimationCurve curve)
    : m_duration(duration), m_curve(curve) {}

void BaseAnimation::start() {
    m_start_time = std::chrono::steady_clock::now();
    m_running = true;
}

bool BaseAnimation::is_running() const noexcept {
    return m_running;
}

double BaseAnimation::interpolate(double t) const noexcept {
    if (t <= 0.0) return 0.0;
    if (t >= 1.0) return 1.0;

    switch (m_curve) {
    case AnimationCurve::Linear:
        return t;
    case AnimationCurve::EaseInCubic:
        return t * t * t;
    case AnimationCurve::EaseOutCubic: {
        double p = t - 1.0;
        return p * p * p + 1.0;
    }
    case AnimationCurve::EaseInOutCubic:
        return (t < 0.5) ? (4.0 * t * t * t) : (1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0);
    }
    return t;
}

void BaseAnimation::tick(std::chrono::steady_clock::time_point now) {
    if (!m_running) return;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    double progress = static_cast<double>(elapsed.count()) / static_cast<double>(m_duration.count());

    if (progress >= 1.0) {
        update(1.0);
        m_running = false;
        on_complete();
    } else {
        update(interpolate(progress));
    }
}

} // namespace tinexus::comp
