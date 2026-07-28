#pragma once

#include <txui/math/Point.hpp>
#include <array>
#include <cmath>

namespace txui {

class Matrix3 {
private:
    // Row-major 3x3 matrix values
    std::array<Scalar, 9> m_data{
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };

public:
    constexpr Matrix3() noexcept = default;

    constexpr Matrix3(
        Scalar m00, Scalar m01, Scalar m02,
        Scalar m10, Scalar m11, Scalar m12,
        Scalar m20, Scalar m21, Scalar m22) noexcept
        : m_data{m00, m01, m02, m10, m11, m12, m20, m21, m22} {}

    [[nodiscard]] static constexpr Matrix3 identity() noexcept {
        return Matrix3();
    }

    [[nodiscard]] static constexpr Matrix3 translation(Coordinate dx, Coordinate dy) noexcept {
        return Matrix3(
            1.0f, 0.0f, dx,
            0.0f, 1.0f, dy,
            0.0f, 0.0f, 1.0f
        );
    }

    [[nodiscard]] static constexpr Matrix3 scale(Scalar sx, Scalar sy) noexcept {
        return Matrix3(
            sx,   0.0f, 0.0f,
            0.0f, sy,   0.0f,
            0.0f, 0.0f, 1.0f
        );
    }

    [[nodiscard]] static Matrix3 rotation(Scalar radians) noexcept {
        Scalar c = std::cos(radians);
        Scalar s = std::sin(radians);
        return Matrix3(
            c,    -s,   0.0f,
            s,     c,   0.0f,
            0.0f, 0.0f, 1.0f
        );
    }

    [[nodiscard]] constexpr Scalar operator[](std::size_t index) const noexcept {
        return m_data[index];
    }

    [[nodiscard]] constexpr Scalar& operator[](std::size_t index) noexcept {
        return m_data[index];
    }

    [[nodiscard]] constexpr Matrix3 operator*(const Matrix3& rhs) const noexcept {
        return Matrix3(
            m_data[0]*rhs.m_data[0] + m_data[1]*rhs.m_data[3] + m_data[2]*rhs.m_data[6],
            m_data[0]*rhs.m_data[1] + m_data[1]*rhs.m_data[4] + m_data[2]*rhs.m_data[7],
            m_data[0]*rhs.m_data[2] + m_data[1]*rhs.m_data[5] + m_data[2]*rhs.m_data[8],

            m_data[3]*rhs.m_data[0] + m_data[4]*rhs.m_data[3] + m_data[5]*rhs.m_data[6],
            m_data[3]*rhs.m_data[1] + m_data[4]*rhs.m_data[4] + m_data[5]*rhs.m_data[7],
            m_data[3]*rhs.m_data[2] + m_data[4]*rhs.m_data[5] + m_data[5]*rhs.m_data[8],

            m_data[6]*rhs.m_data[0] + m_data[7]*rhs.m_data[3] + m_data[8]*rhs.m_data[6],
            m_data[6]*rhs.m_data[1] + m_data[7]*rhs.m_data[4] + m_data[8]*rhs.m_data[7],
            m_data[6]*rhs.m_data[2] + m_data[7]*rhs.m_data[5] + m_data[8]*rhs.m_data[8]
        );
    }

    [[nodiscard]] constexpr Point transform_point(const Point& pt) const noexcept {
        Scalar nx = m_data[0]*pt.x + m_data[1]*pt.y + m_data[2];
        Scalar ny = m_data[3]*pt.x + m_data[4]*pt.y + m_data[5];
        return Point(nx, ny);
    }
};

} // namespace txui
