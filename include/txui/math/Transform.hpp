#pragma once

#include <txui/math/Matrix3.hpp>

namespace txui {

class Transform {
private:
    Matrix3 m_matrix{Matrix3::identity()};

public:
    constexpr Transform() noexcept = default;
    explicit constexpr Transform(const Matrix3& mat) noexcept : m_matrix(mat) {}

    [[nodiscard]] constexpr const Matrix3& matrix() const noexcept { return m_matrix; }

    constexpr Transform& translate(Coordinate dx, Coordinate dy) noexcept {
        m_matrix = m_matrix * Matrix3::translation(dx, dy);
        return *this;
    }

    constexpr Transform& scale(Scalar sx, Scalar sy) noexcept {
        m_matrix = m_matrix * Matrix3::scale(sx, sy);
        return *this;
    }

    Transform& rotate(Scalar radians) noexcept {
        m_matrix = m_matrix * Matrix3::rotation(radians);
        return *this;
    }

    [[nodiscard]] constexpr Point map(const Point& pt) const noexcept {
        return m_matrix.transform_point(pt);
    }
};

} // namespace txui
