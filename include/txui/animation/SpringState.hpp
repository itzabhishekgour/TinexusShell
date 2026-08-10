#pragma once

#include <cmath>

namespace txui {

// ── Spring physics state ───────────────────────────────────────────────────
// Simulates a critically-damped spring: fast, no oscillation, natural feel.
// Client-side implementation based on comp::SpringState.
struct SpringState {
    double value{1.0};
    double velocity{0.0};
    double target{1.0};

    // Step the spring simulation forward by dt seconds.
    // Returns true when the spring has settled (< epsilon from target).
    inline bool step(double dt, double stiffness = 380.0, double damping = 28.0) noexcept {
        double force = -stiffness * (value - target) - damping * velocity;
        velocity += force * dt;
        value    += velocity * dt;

        // Settle threshold
        if (std::abs(value - target) < 0.001 && std::abs(velocity) < 0.001) {
            value    = target;
            velocity = 0.0;
            return true; // settled
        }
        return false;
    }

    inline void reset(double from, double to) noexcept {
        value    = from;
        target   = to;
        velocity = 0.0;
    }
};

} // namespace txui
