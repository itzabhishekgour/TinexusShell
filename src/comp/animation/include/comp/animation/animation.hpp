#ifndef TINEXUS_COMP_ANIMATION_HPP
#define TINEXUS_COMP_ANIMATION_HPP

#include <chrono>
#include <functional>
#include <cmath>

namespace tinexus::comp {

enum class AnimationCurve {
    Linear,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseDecelerate,   // cubic-bezier(0.0, 0.0, 0.2, 1.0) — enter screen
    EaseAccelerate,   // cubic-bezier(0.4, 0.0, 1.0, 1.0) — exit screen
    EaseSpring        // Slight overshoot elastic — used for confirmations/windows
};

// ── Spring physics state ───────────────────────────────────────────────────
// Simulates a critically-damped spring: fast, no oscillation, natural feel.
// stiffness ~200–600, damping ~20–35 for UI use.
struct SpringState {
    double value{0.0};
    double velocity{0.0};
    double target{1.0};

    // Step the spring simulation forward by dt seconds using bounded fixed substeps
    // so explicit integration remains stable across idle wakes and 60Hz–240Hz displays.
    // Returns true when the spring has settled (< epsilon from target).
    bool step(double dt, double stiffness = 380.0, double damping = 28.0,
              double pos_eps = 0.001, double vel_eps = 0.001) noexcept {
        double remaining = std::clamp(dt, 0.0, 0.05);
        constexpr double kMaxSubstep = 1.0 / 240.0;

        while (remaining > 0.0) {
            const double h = std::min(remaining, kMaxSubstep);
            const double force = -stiffness * (value - target) - damping * velocity;
            velocity += force * h;
            value    += velocity * h;
            remaining -= h;
        }

        // Settle threshold
        if (std::abs(value - target) < pos_eps && std::abs(velocity) < vel_eps) {
            value    = target;
            velocity = 0.0;
            return true; // settled
        }
        return false;
    }

    void reset(double from, double to) noexcept {
        value    = from;
        target   = to;
        velocity = 0.0;
    }
};

// ── IAnimation ────────────────────────────────────────────────────────────
class IAnimation {
public:
    virtual ~IAnimation() = default;

    virtual void start()                     = 0;
    virtual void update(double progress)     = 0;
    virtual void on_complete()               = 0;
    [[nodiscard]] virtual bool is_running() const noexcept = 0;
};

// ── BaseAnimation — time-based with easing curves ─────────────────────────
class BaseAnimation : public IAnimation {
public:
    explicit BaseAnimation(std::chrono::milliseconds duration,
                           AnimationCurve curve = AnimationCurve::EaseDecelerate);
    ~BaseAnimation() override = default;

    void start()                              override;
    void update(double progress)              override = 0;
    void on_complete()                        override = 0;
    [[nodiscard]] bool is_running() const noexcept override;

    void tick(std::chrono::steady_clock::time_point now);

protected:
    [[nodiscard]] double interpolate(double t) const noexcept;

private:
    std::chrono::milliseconds               m_duration;
    AnimationCurve                          m_curve;
    std::chrono::steady_clock::time_point   m_start_time{};
    bool                                    m_running{false};
};

// ── SpringAnimation — physics-based, no fixed duration ───────────────────
// Updates via tick(dt). Settles automatically. Feels like iOS/macOS.
class SpringAnimation : public IAnimation {
public:
    // from: starting value, to: target value
    // stiffness: 200 (slow) – 600 (snappy), damping: 20 (bouncy) – 35 (tight)
    SpringAnimation(double from, double to,
                    double stiffness = 380.0, double damping = 28.0);
    ~SpringAnimation() override = default;

    void start()                              override;
    void update(double /*progress*/)          override {} // progress = current spring value [from..to]
    void on_complete()                        override {}

    [[nodiscard]] bool is_running() const noexcept override;

    // Call every frame with delta-time in seconds
    void tick_dt(double dt);

    [[nodiscard]] double current_value() const noexcept { return m_spring.value; }

protected:
    SpringState m_spring;
    double      m_stiffness;
    double      m_damping;
    bool        m_running{false};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_ANIMATION_HPP
