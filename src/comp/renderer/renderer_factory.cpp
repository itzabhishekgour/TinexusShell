#include "comp/renderer/renderer_factory.hpp"
#include "comp/renderer/pixman_renderer.hpp"
#include "comp/renderer/vulkan_renderer.hpp"
#include "comp/renderer/opengl_renderer.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

std::unique_ptr<Renderer> RendererFactory::create_renderer(RendererBackend backend, bool force_vulkan_fail, bool force_opengl_fail) {
    log::info("RendererFactory: Requesting renderer creation for backend '{}'", backend_to_string(backend));

    if (backend == RendererBackend::Auto) {
        log::info("RendererFactory: Auto-detection active: Trying Vulkan -> OpenGL -> Pixman fallback ladder...");
        
        // 1. Try Vulkan
        if (!force_vulkan_fail) {
            auto vulkan = std::make_unique<VulkanRenderer>();
            if (vulkan->initialize()) {
                log::info("RendererFactory: Successfully instantiated VulkanRenderer (Vulkan GPU Acceleration Active)");
                return vulkan;
            }
            log::warn("RendererFactory: Vulkan initialization failed (VK_ERROR_INITIALIZATION_FAILED), falling back to OpenGL...");
        } else {
            log::warn("RendererFactory: Vulkan forced failure simulation, falling back to OpenGL...");
        }

        // 2. Try OpenGL
        if (!force_opengl_fail) {
            auto gl = std::make_unique<OpenGLRenderer>();
            if (gl->initialize()) {
                log::info("RendererFactory: Successfully instantiated OpenGLRenderer (EGL Context Active)");
                return gl;
            }
            log::warn("RendererFactory: OpenGL/EGL initialization failed, falling back to Pixman software renderer...");
        } else {
            log::warn("RendererFactory: OpenGL forced failure simulation, falling back to Pixman software renderer...");
        }

        // 3. Fallback to Pixman
        log::info("RendererFactory: Falling back to Pixman software renderer");
        return std::make_unique<PixmanRenderer>();
    }

    switch (backend) {
        case RendererBackend::Vulkan: {
            if (!force_vulkan_fail) {
                auto r = std::make_unique<VulkanRenderer>();
                if (r->initialize()) return r;
            }
            log::warn("RendererFactory: Explicit Vulkan renderer failed, falling back to Pixman");
            return std::make_unique<PixmanRenderer>();
        }
        case RendererBackend::OpenGL: {
            if (!force_opengl_fail) {
                auto r = std::make_unique<OpenGLRenderer>();
                if (r->initialize()) return r;
            }
            log::warn("RendererFactory: Explicit OpenGL renderer failed, falling back to Pixman");
            return std::make_unique<PixmanRenderer>();
        }
        case RendererBackend::Pixman:
        case RendererBackend::Headless:
        case RendererBackend::DRM:
        default:
            return std::make_unique<PixmanRenderer>();
    }
}

RendererBackend RendererFactory::backend_from_string(const std::string& name) noexcept {
    if (name == "auto") return RendererBackend::Auto;
    if (name == "vulkan") return RendererBackend::Vulkan;
    if (name == "gles" || name == "opengl") return RendererBackend::OpenGL;
    if (name == "drm" || name == "kms") return RendererBackend::DRM;
    if (name == "headless") return RendererBackend::Headless;
    return RendererBackend::Pixman;
}

std::string RendererFactory::backend_to_string(RendererBackend backend) {
    switch (backend) {
        case RendererBackend::Auto: return "Auto";
        case RendererBackend::Vulkan: return "Vulkan";
        case RendererBackend::OpenGL: return "OpenGL";
        case RendererBackend::DRM: return "DRM/KMS";
        case RendererBackend::Headless: return "Headless";
        case RendererBackend::Pixman: default: return "Pixman";
    }
}

} // namespace tinexus::comp
