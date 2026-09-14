#ifndef TINEXUS_COMP_RENDERER_BLUR_PASS_HPP
#define TINEXUS_COMP_RENDERER_BLUR_PASS_HPP

#include "comp/renderer/render_surface.hpp"
#include "comp/render/damage_tracker.hpp"
#include <cstdint>
#include <memory>

struct wlr_texture;

namespace tinexus::comp {

class BlurPass {
public:
    BlurPass();
    ~BlurPass();

    bool initialize(uint32_t width, uint32_t height);
    void shutdown();

    // Performs Dual-Kawase multi-pass downsample/upsample blur on an in-memory ARGB pixel buffer.
    static void apply_dual_kawase(uint32_t* pixels, int width, int height, int stride_pixels, int iterations = 3, int radius = 24);

    // Extracts a sub-box from source pixels, runs Dual-Kawase blur, applies tint, and stores in dst.
    static bool blur_box(const uint32_t* src_pixels, int src_w, int src_h, int src_stride_pixels,
                         uint32_t* dst_pixels, int dst_stride_pixels,
                         int box_x, int box_y, int box_w, int box_h,
                         int iterations = 3, uint32_t tint_rgba = 0x13131ACC);

    // Performs blur on a given texture region.
    struct wlr_texture* apply_blur(struct wlr_texture* source_texture,
                                   const DamageRegion& region,
                                   int32_t radius,
                                   uint32_t tint_rgba);

    // Executes the blur pipeline on a RenderSurface.
    void execute_blur_pipeline(RenderSurface& surface);

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    bool m_initialized{false};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_BLUR_PASS_HPP
