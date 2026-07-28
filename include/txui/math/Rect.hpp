#pragma once

#include <txui/math/Point.hpp>
#include <txui/math/Size.hpp>

namespace txui {

struct Rect {
    Point origin{};
    Size size{};

    constexpr Rect() noexcept = default;
    constexpr Rect(Coordinate x, Coordinate y, Coordinate width, Coordinate height) noexcept
        : origin(x, y), size(width, height) {}
    constexpr Rect(const Point& pt, const Size& sz) noexcept
        : origin(pt), size(sz) {}

    [[nodiscard]] constexpr Coordinate x() const noexcept { return origin.x; }
    [[nodiscard]] constexpr Coordinate y() const noexcept { return origin.y; }
    [[nodiscard]] constexpr Coordinate width() const noexcept { return size.width; }
    [[nodiscard]] constexpr Coordinate height() const noexcept { return size.height; }

    [[nodiscard]] constexpr Coordinate left() const noexcept { return origin.x; }
    [[nodiscard]] constexpr Coordinate top() const noexcept { return origin.y; }
    [[nodiscard]] constexpr Coordinate right() const noexcept { return origin.x + size.width; }
    [[nodiscard]] constexpr Coordinate bottom() const noexcept { return origin.y + size.height; }

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return size.is_empty();
    }

    [[nodiscard]] constexpr bool contains(const Point& pt) const noexcept {
        return pt.x >= left() && pt.x < right() && pt.y >= top() && pt.y < bottom();
    }

    [[nodiscard]] constexpr bool contains(Coordinate px, Coordinate py) const noexcept {
        return px >= left() && px < right() && py >= top() && py < bottom();
    }

    [[nodiscard]] constexpr bool intersects(const Rect& other) const noexcept {
        return !(other.left() >= right() || other.right() <= left() ||
                 other.top() >= bottom() || other.bottom() <= top());
    }

    [[nodiscard]] constexpr bool operator==(const Rect& rhs) const noexcept {
        return origin == rhs.origin && size == rhs.size;
    }

    [[nodiscard]] constexpr bool operator!=(const Rect& rhs) const noexcept {
        return !(*this == rhs);
    }
};

} // namespace txui
