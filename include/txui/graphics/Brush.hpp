#pragma once

#include <txui/graphics/Color.hpp>
#include <variant>

namespace txui {

// Phase 4.2.1: Only SolidBrush is supported.
// Under Rule #1 (Zero Placeholder Policy), Gradient/ImagePattern will be added
// only when their full rasterization pipeline is implemented.
class SolidBrush {
private:
    Color m_color{Color::white()};

public:
    constexpr SolidBrush() noexcept = default;
    explicit constexpr SolidBrush(const Color& c) noexcept : m_color(c) {}

    [[nodiscard]] constexpr const Color& color() const noexcept { return m_color; }

    [[nodiscard]] constexpr bool operator==(const SolidBrush& rhs) const noexcept {
        return m_color == rhs.m_color;
    }

    [[nodiscard]] constexpr bool operator!=(const SolidBrush& rhs) const noexcept {
        return !(*this == rhs);
    }
};

using Brush = std::variant<SolidBrush>;

} // namespace txui
