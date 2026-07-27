#ifndef TINEXUS_COMP_RENDER_BUFFER_HPP
#define TINEXUS_COMP_RENDER_BUFFER_HPP

#include <cstdint>

namespace tinexus::comp {

struct RenderBuffer {
    void* pixels{nullptr};
    int32_t width{0};
    int32_t height{0};
    int32_t stride{0};
    int32_t format{0}; // 0: ARGB8888, 1: XRGB8888
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDER_BUFFER_HPP
