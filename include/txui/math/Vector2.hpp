#pragma once

#include <txui/core/Types.hpp>
#include <cmath>

namespace txui {

struct Vector2 {
    Scalar x{0.0f};
    Scalar y{0.0f};

    constexpr Vector2() noexcept = default;
    constexpr Vector2(Scalar vx, Scalar vy) noexcept : x(vx), y(vy) {}

    [[nodiscard]] constexpr Scalar dot(const Vector2& rhs) const noexcept {
        return x * rhs.x + y * rhs.y;
    }

    [[nodiscard]] Scalar length() const noexcept {
        return std::sqrt(dot(*this));
    }

    [[nodiscard]] Vector2 normalized() const noexcept {
        Scalar len = length();
        if (len > 0.00001f) {
            return Vector2(x / len, y / len);
        }
        return Vector2(0.0f, 0.0f);
    }

    [[nodiscard]] constexpr Vector2 operator+(const Vector2& rhs) const noexcept {
        return Vector2(x + rhs.x, y + rhs.y);
    }

    [[nodiscard]] constexpr Vector2 operator-(const Vector2& rhs) const noexcept {
        return Vector2(x - rhs.x, y - rhs.y);
    }

    [[nodiscard]] constexpr Vector2 operator*(Scalar s) const noexcept {
        return Vector2(x * s, y * s);
    }
};

} // namespace txui
