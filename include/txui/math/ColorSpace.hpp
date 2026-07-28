#pragma once

#include <txui/core/Types.hpp>
#include <cmath>

namespace txui {

enum class ColorSpaceType {
    sRGB,
    LinearRGB,
    DisplayP3
};

class ColorSpace {
public:
    [[nodiscard]] static float32 srgb_to_linear(float32 c) noexcept {
        return (c <= 0.04045f) ? (c / 12.92f) : std::pow((c + 0.055f) / 1.055f, 2.4f);
    }

    [[nodiscard]] static float32 linear_to_srgb(float32 c) noexcept {
        return (c <= 0.0031308f) ? (12.92f * c) : (1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f);
    }
};

} // namespace txui
