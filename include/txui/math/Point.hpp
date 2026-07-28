#pragma once

#include <txui/core/Types.hpp>

namespace txui {

struct Point {
    Coordinate x{0.0f};
    Coordinate y{0.0f};

    constexpr Point() noexcept = default;
    constexpr Point(Coordinate px, Coordinate py) noexcept : x(px), y(py) {}

    [[nodiscard]] constexpr Point operator+(const Point& rhs) const noexcept {
        return Point(x + rhs.x, y + rhs.y);
    }

    [[nodiscard]] constexpr Point operator-(const Point& rhs) const noexcept {
        return Point(x - rhs.x, y - rhs.y);
    }

    constexpr Point& operator+=(const Point& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    constexpr Point& operator-=(const Point& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Point& rhs) const noexcept {
        return x == rhs.x && y == rhs.y;
    }

    [[nodiscard]] constexpr bool operator!=(const Point& rhs) const noexcept {
        return !(*this == rhs);
    }
};

} // namespace txui
