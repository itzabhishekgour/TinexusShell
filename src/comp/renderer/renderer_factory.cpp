#include "comp/renderer/renderer_factory.hpp"
#include "comp/renderer/pixman_renderer.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

std::unique_ptr<Renderer> RendererFactory::create_renderer(RendererBackend backend) {
    log::info("RendererFactory: Creating renderer for backend '{}'", backend_to_string(backend));
    switch (backend) {
        case RendererBackend::Pixman:
        case RendererBackend::Headless:
        case RendererBackend::Vulkan:
        case RendererBackend::OpenGL:
        case RendererBackend::DRM:
        default:
            return std::make_unique<PixmanRenderer>();
    }
}

RendererBackend RendererFactory::backend_from_string(const std::string& name) noexcept {
    if (name == "vulkan") return RendererBackend::Vulkan;
    if (name == "gles" || name == "opengl") return RendererBackend::OpenGL;
    if (name == "drm" || name == "kms") return RendererBackend::DRM;
    if (name == "headless") return RendererBackend::Headless;
    return RendererBackend::Pixman;
}

std::string RendererFactory::backend_to_string(RendererBackend backend) {
    switch (backend) {
        case RendererBackend::Vulkan: return "Vulkan";
        case RendererBackend::OpenGL: return "OpenGL";
        case RendererBackend::DRM: return "DRM/KMS";
        case RendererBackend::Headless: return "Headless";
        case RendererBackend::Pixman: default: return "Pixman";
    }
}

} // namespace tinexus::comp
