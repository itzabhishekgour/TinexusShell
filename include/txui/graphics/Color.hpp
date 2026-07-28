#pragma once

#include <txui/core/Types.hpp>

namespace txui {

class Color {
private:
    uint8 m_r{0};
    uint8 m_g{0};
    uint8 m_b{0};
    uint8 m_a{255};

public:
    constexpr Color() noexcept = default;
    constexpr Color(uint8 red, uint8 green, uint8 blue, uint8 alpha = 255) noexcept
        : m_r(red), m_g(green), m_b(blue), m_a(alpha) {}

    // Immutable getters
    [[nodiscard]] constexpr uint8 r() const noexcept { return m_r; }
    [[nodiscard]] constexpr uint8 g() const noexcept { return m_g; }
    [[nodiscard]] constexpr uint8 b() const noexcept { return m_b; }
    [[nodiscard]] constexpr uint8 a() const noexcept { return m_a; }

    // Immutable transformation returning new Color instance
    [[nodiscard]] constexpr Color with_alpha(uint8 new_alpha) const noexcept {
        return Color(m_r, m_g, m_b, new_alpha);
    }

    [[nodiscard]] constexpr uint32 to_argb32() const noexcept {
        return (static_cast<uint32>(m_a) << 24) |
               (static_cast<uint32>(m_r) << 16) |
               (static_cast<uint32>(m_g) << 8)  |
               static_cast<uint32>(m_b);
    }

    [[nodiscard]] constexpr uint32 to_argb32_premultiplied() const noexcept {
        uint32 pr = (static_cast<uint32>(m_r) * static_cast<uint32>(m_a)) / 255U;
        uint32 pg = (static_cast<uint32>(m_g) * static_cast<uint32>(m_a)) / 255U;
        uint32 pb = (static_cast<uint32>(m_b) * static_cast<uint32>(m_a)) / 255U;
        return (static_cast<uint32>(m_a) << 24) | (pr << 16) | (pg << 8) | pb;
    }

    [[nodiscard]] constexpr uint32 to_rgba32() const noexcept {
        return (static_cast<uint32>(m_r) << 24) |
               (static_cast<uint32>(m_g) << 16) |
               (static_cast<uint32>(m_b) << 8)  |
               static_cast<uint32>(m_a);
    }

    [[nodiscard]] static constexpr Color red()   noexcept { return Color(255, 0, 0, 255); }
    [[nodiscard]] static constexpr Color green() noexcept { return Color(0, 255, 0, 255); }
    [[nodiscard]] static constexpr Color blue()  noexcept { return Color(0, 0, 255, 255); }
    [[nodiscard]] static constexpr Color white() noexcept { return Color(255, 255, 255, 255); }
    [[nodiscard]] static constexpr Color black() noexcept { return Color(0, 0, 0, 255); }
    [[nodiscard]] static constexpr Color transparent() noexcept { return Color(0, 0, 0, 0); }

    [[nodiscard]] constexpr bool operator==(const Color& rhs) const noexcept {
        return m_r == rhs.m_r && m_g == rhs.m_g && m_b == rhs.m_b && m_a == rhs.m_a;
    }

    [[nodiscard]] constexpr bool operator!=(const Color& rhs) const noexcept {
        return !(*this == rhs);
    }
};

} // namespace txui
