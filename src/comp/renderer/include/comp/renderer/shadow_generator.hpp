#pragma once

#include <cstdint>
#include <pixman.h>

namespace tinexus::comp::renderer {

struct ShadowTexture {
    pixman_image_t* image{nullptr};
    int32_t offset_x{0};
    int32_t offset_y{0};
    int32_t width{0};
    int32_t height{0};
};

ShadowTexture generate_shadow(int32_t win_width, int32_t win_height, int32_t radius, uint8_t alpha);

} // namespace tinexus::comp::renderer
