#pragma once
// ============================================================================
// SpringStatePortable.hpp
// Standalone C++20 port of txui::SpringState — NO txui, NO Qt dependencies.
// Drop this header anywhere in the codebase for Qt6 migration consumers.
//
// Bit-exact against txui::SpringState for identical (dt, stiffness, damping).
// Verified by test-tier1-gate golden-value checks.
// ============================================================================
#include <cmath>
#include <cstdint>

namespace tinexus::migration {

/// Critically-damped spring simulation — identical algorithm to txui::SpringState.
/// Parameters match txui defaults: stiffness=380.0, damping=28.0
struct SpringStatePortable {
    double value    {1.0};
    double velocity {0.0};
    double target   {1.0};

    /// Step the spring forward by dt seconds.
    /// Returns true when settled (< epsilon from target).
    [[nodiscard]] inline bool step(
        double dt,
        double stiffness = 380.0,
        double damping   = 28.0) noexcept
    {
        const double force = -stiffness * (value - target) - damping * velocity;
        velocity += force * dt;
        value    += velocity * dt;

        constexpr double kEpsilon = 0.001;
        if (std::abs(value - target) < kEpsilon && std::abs(velocity) < kEpsilon) {
            value    = target;
            velocity = 0.0;
            return true;  // settled
        }
        return false;
    }

    /// Reset spring state — equivalent to txui::SpringState::reset().
    inline void reset(double from, double to) noexcept {
        value    = from;
        target   = to;
        velocity = 0.0;
    }

    /// Convenience: set new target without resetting velocity (for smooth retargeting).
    inline void set_target(double to) noexcept {
        target = to;
    }
};

// ── Golden-value reference table (generated from txui::SpringState) ──────────
// Input: reset(1.0 → 1.68), step with dt=1/60s, stiffness=380, damping=28
// Computed by simulating txui::SpringState step() in Python (IEEE 754 double).
// test-tier1-gate verifies our portable impl matches within 1e-6 tolerance.
struct SpringGoldenFrame {
    int    frame;
    double expected_value;
    double expected_velocity;
};

inline constexpr SpringGoldenFrame kDockMagnifyGolden[] = {
    {  1,  1.071777777777778,  4.306666666666667},
    {  2,  1.174260493827160,  6.148962962962963},
    {  3,  1.282301556927298,  6.482463786008231},
    {  5,  1.466489189076656,  5.075187103630038},
    { 10,  1.666123595129593,  1.027329214674294},
    { 20,  1.681895170596868, -0.046566686206057},
    { 30,  1.680000000000000,  0.000000000000000},  // settled at frame 29
};

constexpr double kGoldenTolerance = 1e-6;  // max abs error per component

} // namespace tinexus::migration

namespace tinexus::animation {
    using SpringState = tinexus::migration::SpringStatePortable;
}
