#include "comp/animation/animation.hpp"
#include <cmath>

namespace tinexus::comp {

// ─────────────────────────────────────────────────────────────────────────────
// BaseAnimation
// ─────────────────────────────────────────────────────────────────────────────

BaseAnimation::BaseAnimation(std::chrono::milliseconds duration, AnimationCurve curve)
    : m_duration(duration), m_curve(curve) {}

void BaseAnimation::start() {
    m_start_time = std::chrono::steady_clock::now();
    m_running    = true;
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
        return (t < 0.5) ? (4.0 * t * t * t)
                         : (1.0 - std::pow(-2.0 * t + 2.0, 3.0) / 2.0);

    // cubic-bezier(0.0, 0.0, 0.2, 1.0) — decelerate: fast in, gentle landing
    case AnimationCurve::EaseDecelerate: {
        // Approximated via a strong ease-out quintic
        double p = t - 1.0;
        return p * p * p * p * p + 1.0;
    }

    // cubic-bezier(0.4, 0.0, 1.0, 1.0) — accelerate: gentle leave, fast exit
    case AnimationCurve::EaseAccelerate:
        return t * t * t * t;

    // Slight overshoot spring approximation (cubic-bezier(0.34, 1.56, 0.64, 1.0))
    case AnimationCurve::EaseSpring: {
        // Approximate spring with an overshoot polynomial
        // Peaks at ~1.1 around t=0.7, then settles to 1.0
        if (t < 0.7) {
            return 2.2 * t * t;
        } else {
            double u = t - 1.0;
            return 1.0 + 0.15 * u * u * (3.0 + 2.0 * u);
        }
    }
    }
    return t;
}

void BaseAnimation::tick(std::chrono::steady_clock::time_point now) {
    if (!m_running) return;

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    double progress = static_cast<double>(elapsed.count()) /
                      static_cast<double>(m_duration.count());

    if (progress >= 1.0) {
        update(1.0);
        m_running = false;
        on_complete();
    } else {
        update(interpolate(progress));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SpringAnimation
// ─────────────────────────────────────────────────────────────────────────────

SpringAnimation::SpringAnimation(double from, double to,
                                 double stiffness, double damping)
    : m_stiffness(stiffness), m_damping(damping) {
    m_spring.reset(from, to);
}

void SpringAnimation::start() {
    m_running = true;
}

bool SpringAnimation::is_running() const noexcept {
    return m_running;
}

void SpringAnimation::tick_dt(double dt) {
    if (!m_running) return;

    bool settled = m_spring.step(dt, m_stiffness, m_damping);
    update(m_spring.value);

    if (settled) {
        m_running = false;
        on_complete();
    }
}

} // namespace tinexus::comp
