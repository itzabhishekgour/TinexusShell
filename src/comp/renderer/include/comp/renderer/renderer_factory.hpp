#ifndef TINEXUS_COMP_RENDERER_FACTORY_HPP
#define TINEXUS_COMP_RENDERER_FACTORY_HPP

#include "comp/renderer/renderer.hpp"
#include <memory>
#include <string>

namespace tinexus::comp {

enum class RendererBackend {
    Auto,
    Pixman,
    Vulkan,
    OpenGL,
    DRM,
    Headless
};

class RendererFactory {
public:
    static std::unique_ptr<Renderer> create_renderer(RendererBackend backend = RendererBackend::Auto, bool force_vulkan_fail = false, bool force_opengl_fail = false);
    static RendererBackend backend_from_string(const std::string& name) noexcept;
    static std::string backend_to_string(RendererBackend backend);
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_FACTORY_HPP
