#pragma once

#include <txui/core/Types.hpp>
#include <txui/math/Size.hpp>
#include <algorithm>
#include <limits>

namespace txui {

constexpr float64 INF = std::numeric_limits<float64>::infinity();

struct Constraints {
    float64 min_width{0.0};
    float64 max_width{0.0};
    float64 min_height{0.0};
    float64 max_height{0.0};

    constexpr Constraints() noexcept = default;
    
    constexpr Constraints(float64 min_w, float64 max_w, float64 min_h, float64 max_h) noexcept
        : min_width(std::max(0.0, min_w)),
          max_width(std::max(min_width, max_w)),
          min_height(std::max(0.0, min_h)),
          max_height(std::max(min_height, max_h)) {}

    // Checks if the constraints force exactly one size
    [[nodiscard]] constexpr bool is_tight() const noexcept {
        return min_width == max_width && min_height == max_height;
    }

    // Checks if the constraints have an upper bound
    [[nodiscard]] constexpr bool is_bounded() const noexcept {
        return max_width < INF && max_height < INF;
    }

    [[nodiscard]] constexpr bool is_bounded_width() const noexcept {
        return max_width < INF;
    }

    [[nodiscard]] constexpr bool is_bounded_height() const noexcept {
        return max_height < INF;
    }

    // Forces a given size to fit within these constraints
    [[nodiscard]] constexpr Size constrain(const Size& size) const noexcept {
        return Size(
            std::clamp(size.width, min_width, max_width),
            std::clamp(size.height, min_height, max_height)
        );
    }

    // Returns new constraints where min bounds are clamped to max bounds
    [[nodiscard]] constexpr Constraints tighten(float64 width, float64 height) const noexcept {
        return Constraints(
            std::clamp(width, min_width, max_width),
            std::clamp(width, min_width, max_width),
            std::clamp(height, min_height, max_height),
            std::clamp(height, min_height, max_height)
        );
    }

    // Returns new constraints removing minimum bounds
    [[nodiscard]] constexpr Constraints loosen() const noexcept {
        return Constraints(0.0, max_width, 0.0, max_height);
    }

    // Factory for unbounded constraints
    [[nodiscard]] static constexpr Constraints unbounded() noexcept {
        return Constraints(0.0, INF, 0.0, INF);
    }

    // Factory for exact dimensions
    [[nodiscard]] static constexpr Constraints tight(float64 width, float64 height) noexcept {
        return Constraints(width, width, height, height);
    }

    // Factory for exact size
    [[nodiscard]] static constexpr Constraints tight_for(const Size& size) noexcept {
        return tight(size.width, size.height);
    }

    [[nodiscard]] constexpr bool operator==(const Constraints& other) const noexcept {
        return min_width == other.min_width && max_width == other.max_width &&
               min_height == other.min_height && max_height == other.max_height;
    }
};

} // namespace txui
