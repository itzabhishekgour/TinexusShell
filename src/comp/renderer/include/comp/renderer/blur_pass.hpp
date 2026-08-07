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

    // Performs a two-pass Gaussian blur on a given texture region.
    // In production, this binds a Vulkan compute shader pipeline or GLES shader,
    // runs a horizontal pass into a temp texture, and a vertical pass back.
    struct wlr_texture* apply_blur(struct wlr_texture* source_texture,
                                   const DamageRegion& region,
                                   int32_t radius,
                                   uint32_t tint_rgba);

    // Simulates the architectural execution of backdrop blur for a RenderSurface.
    void execute_blur_pipeline(RenderSurface& surface);

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    bool m_initialized{false};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_BLUR_PASS_HPP
