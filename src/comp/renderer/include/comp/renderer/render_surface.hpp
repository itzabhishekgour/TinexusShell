#ifndef TINEXUS_COMP_RENDER_SURFACE_HPP
#define TINEXUS_COMP_RENDER_SURFACE_HPP

#include "comp/renderer/render_buffer.hpp"
#include <memory>

namespace tinexus::comp {

struct DamageRect {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};
};

class RenderSurface {
public:
    RenderSurface(uint64_t surface_id);
    ~RenderSurface() = default;

    uint64_t id{0};
    int32_t x{0};
    int32_t y{0};
    float opacity{1.0f};
    float scale{1.0f};

    DamageRect damage;
    std::shared_ptr<RenderBuffer> buffer;

    bool has_blur{false};
    int32_t blur_radius{0};
    uint32_t blur_tint{0x00000000}; // RGBA
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDER_SURFACE_HPP
