#ifndef TINEXUS_COMP_RENDERER_HPP
#define TINEXUS_COMP_RENDERER_HPP

#include "comp/renderer/render_surface.hpp"
#include "comp/render/damage_tracker.hpp"
#include <cstdint>

namespace tinexus::comp {

enum class RendererBackend;

class Renderer {
public:
    virtual ~Renderer() = default;

    virtual bool initialize(uint32_t width, uint32_t height) = 0;
    virtual void begin_frame() = 0;
    virtual void compose_surface(RenderSurface& surface) = 0;
    virtual void damage_region(const DamageRegion& region) = 0;
    virtual void end_frame() = 0;
    virtual void present() = 0;
    [[nodiscard]] virtual RendererBackend backend_type() const noexcept = 0;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_HPP
