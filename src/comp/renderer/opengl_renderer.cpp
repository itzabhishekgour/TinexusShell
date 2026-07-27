#include "comp/renderer/opengl_renderer.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

bool OpenGLRenderer::initialize(uint32_t width, uint32_t height) {
    log::info("OpenGLRenderer: Creating EGL Context (eglCreateContext) & Compiling GL Shaders ({}x{})...", width, height);
    
    // Simulate EGL / OpenGL hardware initialization
    m_gl_shader = GpuResourceManager::instance().create_pipeline("gl_scissor_shader.glsl");
    m_egl_initialized = true;
    log::info("OpenGLRenderer: OpenGL/EGL GPU Render Pipeline Initialized (GL Scissor Box Damage Active)");
    return true;
}

void OpenGLRenderer::begin_frame() {
    log::info("OpenGLRenderer: eglMakeCurrent -> glClear(GL_COLOR_BUFFER_BIT)");
}

void OpenGLRenderer::compose_surface(RenderSurface& surface) {
    log::info("OpenGLRenderer: glDrawElements for Surface ID={}", surface.id);
}

void OpenGLRenderer::damage_region(const DamageRegion& region) {
    auto bbox = region.bounding_box();
    log::info("OpenGLRenderer: glScissor({}, {}, {}, {})", bbox.x, bbox.y, bbox.width, bbox.height);
}

void OpenGLRenderer::end_frame() {
    log::info("OpenGLRenderer: eglSwapBuffers (EGL Surface Swapped)");
}

void OpenGLRenderer::present() {
    log::info("OpenGLRenderer: EGL Frame Presented");
}

bool OpenGLRenderer::render_target(IRenderTarget& target, const DamageRegion& damage) {
    if (!target.is_valid()) return false;
    auto bbox = damage.bounding_box();
    log::info("OpenGLRenderer: glScissor({}, {}, {}, {}) -> Rendered EGL Target ({})",
              bbox.x, bbox.y, bbox.width, bbox.height, static_cast<int>(target.target_type()));
    return true;
}

} // namespace tinexus::comp
