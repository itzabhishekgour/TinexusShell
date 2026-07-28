#pragma once

#include <cstdint>

namespace ui {

struct Color {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
    uint8_t a{255};

    // Helper to get pre-multiplied ARGB8888 as expected by Wayland WL_SHM_FORMAT_ARGB8888
    uint32_t to_argb8888() const {
        uint8_t pre_r = static_cast<uint8_t>((r * a) / 255);
        uint8_t pre_g = static_cast<uint8_t>((g * a) / 255);
        uint8_t pre_b = static_cast<uint8_t>((b * a) / 255);
        return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(pre_r) << 16) | (static_cast<uint32_t>(pre_g) << 8) | static_cast<uint32_t>(pre_b);
    }
};

} // namespace ui
