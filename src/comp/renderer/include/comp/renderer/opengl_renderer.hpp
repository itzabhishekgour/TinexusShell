#ifndef TINEXUS_COMP_RENDERER_OPENGL_RENDERER_HPP
#define TINEXUS_COMP_RENDERER_OPENGL_RENDERER_HPP

#include "comp/renderer/renderer.hpp"
#include "comp/renderer/renderer_factory.hpp"
#include "comp/renderer/render_target.hpp"
#include "comp/renderer/gpu_resource_manager.hpp"

namespace tinexus::comp {

class OpenGLRenderer : public Renderer {
public:
    OpenGLRenderer() = default;
    ~OpenGLRenderer() override = default;

    bool initialize(uint32_t width, uint32_t height) override;
    bool initialize();
    void begin_frame() override;
    void compose_surface(RenderSurface& surface) override;
    void damage_region(const DamageRegion& region) override;
    void end_frame() override;
    void present() override;
    [[nodiscard]] RendererBackend backend_type() const noexcept override { return RendererBackend::OpenGL; }

    [[nodiscard]] bool is_egl_initialized() const noexcept { return m_egl_initialized; }
    bool render_target(IRenderTarget& target, const DamageRegion& damage);

private:
    bool m_egl_initialized{false};
    GpuResourceHandle m_gl_shader;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_OPENGL_RENDERER_HPP
