#pragma once

#include <txui/core/Types.hpp>

namespace txui {

struct Insets {
    Coordinate top{0.0f};
    Coordinate right{0.0f};
    Coordinate bottom{0.0f};
    Coordinate left{0.0f};

    constexpr Insets() noexcept = default;
    constexpr Insets(Coordinate all) noexcept
        : top(all), right(all), bottom(all), left(all) {}
    constexpr Insets(Coordinate vertical, Coordinate horizontal) noexcept
        : top(vertical), right(horizontal), bottom(vertical), left(horizontal) {}
    constexpr Insets(Coordinate t, Coordinate r, Coordinate b, Coordinate l) noexcept
        : top(t), right(r), bottom(b), left(l) {}

    [[nodiscard]] constexpr Coordinate horizontal() const noexcept {
        return left + right;
    }

    [[nodiscard]] constexpr Coordinate vertical() const noexcept {
        return top + bottom;
    }

    [[nodiscard]] constexpr bool operator==(const Insets& rhs) const noexcept {
        return top == rhs.top && right == rhs.right && bottom == rhs.bottom && left == rhs.left;
    }

    [[nodiscard]] constexpr bool operator!=(const Insets& rhs) const noexcept {
        return !(*this == rhs);
    }
};

} // namespace txui
