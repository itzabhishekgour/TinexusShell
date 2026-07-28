#pragma once

#include <txui/core/Types.hpp>

namespace txui {

struct Size {
    Coordinate width{0.0};
    Coordinate height{0.0};

    constexpr Size() noexcept = default;
    constexpr Size(Coordinate w, Coordinate h) noexcept : width(w), height(h) {}

    [[nodiscard]] constexpr bool is_empty() const noexcept {
        return width <= 0.0 || height <= 0.0;
    }

    [[nodiscard]] constexpr Coordinate area() const noexcept {
        return (width > 0.0 && height > 0.0) ? (width * height) : 0.0;
    }

    [[nodiscard]] constexpr bool operator==(const Size& rhs) const noexcept {
        return width == rhs.width && height == rhs.height;
    }

    [[nodiscard]] constexpr bool operator!=(const Size& rhs) const noexcept {
        return !(*this == rhs);
    }
};

} // namespace txui
