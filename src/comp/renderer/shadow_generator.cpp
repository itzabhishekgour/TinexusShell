#include <comp/renderer/shadow_generator.hpp>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cstdlib>

namespace tinexus::comp::renderer {

static void box_blur_h(uint32_t* src, uint32_t* dst, int width, int height, int radius) {
    if (radius <= 0) return;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int count = 0;
            int a_sum = 0; // Shadow is just black, so we only need alpha
            
            for (int dx = -radius; dx <= radius; dx++) {
                int px = std::clamp(x + dx, 0, width - 1);
                uint32_t pixel = src[y * width + px];
                a_sum += (pixel >> 24) & 0xFF;
                count++;
            }
            
            dst[y * width + x] = ((a_sum / count) << 24);
        }
    }
}

static void box_blur_v(uint32_t* src, uint32_t* dst, int width, int height, int radius) {
    if (radius <= 0) return;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int count = 0;
            int a_sum = 0;
            
            for (int dy = -radius; dy <= radius; dy++) {
                int py = std::clamp(y + dy, 0, height - 1);
                uint32_t pixel = src[py * width + x];
                a_sum += (pixel >> 24) & 0xFF;
                count++;
            }
            
            dst[y * width + x] = ((a_sum / count) << 24);
        }
    }
}

ShadowTexture generate_shadow(int32_t win_width, int32_t win_height, int32_t radius, uint8_t alpha) {
    ShadowTexture tex;
    tex.width = win_width + 2 * radius;
    tex.height = win_height + 2 * radius;
    tex.offset_x = -radius;
    tex.offset_y = -radius;

    if (win_width <= 0 || win_height <= 0 || radius <= 0) {
        return tex;
    }

    std::vector<uint32_t> buf1(tex.width * tex.height, 0);
    std::vector<uint32_t> buf2(tex.width * tex.height, 0);

    uint32_t shadow_color = (alpha << 24);
    for (int y = radius; y < radius + win_height; y++) {
        for (int x = radius; x < radius + win_width; x++) {
            buf1[y * tex.width + x] = shadow_color;
        }
    }

    int box_radius = radius / 3;
    if (box_radius < 1) box_radius = 1;

    box_blur_h(buf1.data(), buf2.data(), tex.width, tex.height, box_radius);
    box_blur_v(buf2.data(), buf1.data(), tex.width, tex.height, box_radius);
    
    box_blur_h(buf1.data(), buf2.data(), tex.width, tex.height, box_radius);
    box_blur_v(buf2.data(), buf1.data(), tex.width, tex.height, box_radius);

    box_blur_h(buf1.data(), buf2.data(), tex.width, tex.height, box_radius);
    box_blur_v(buf2.data(), buf1.data(), tex.width, tex.height, box_radius);

    uint32_t* final_data = (uint32_t*)std::malloc(tex.width * tex.height * 4);
    if (final_data) {
        std::memcpy(final_data, buf1.data(), tex.width * tex.height * 4);
    }

    tex.image = pixman_image_create_bits(PIXMAN_a8r8g8b8, tex.width, tex.height, 
                                         final_data, tex.width * 4);
                                         
    pixman_image_set_destroy_function(tex.image, [](pixman_image_t* image, void* data) {
        std::free(data);
    }, final_data);

    return tex;
}

} // namespace tinexus::comp::renderer
