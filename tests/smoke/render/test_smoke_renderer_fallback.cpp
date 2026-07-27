#include <cassert>
#include <iostream>
#include "comp/renderer/renderer_factory.hpp"
#include "comp/renderer/vulkan_renderer.hpp"
#include "comp/renderer/opengl_renderer.hpp"
#include "comp/renderer/pixman_renderer.hpp"

using namespace tinexus::comp;

int main() {
    std::cout << "[+] Running smoke_renderer_fallback test suite..." << std::endl;

    // 1. Normal Auto Mode -> Vulkan Primary
    auto renderer_vulkan = RendererFactory::create_renderer(RendererBackend::Auto, false, false);
    assert(renderer_vulkan != nullptr);
    assert(renderer_vulkan->backend_type() == RendererBackend::Vulkan);

    // 2. Vulkan Fails -> Fallback to OpenGL
    auto renderer_gl = RendererFactory::create_renderer(RendererBackend::Auto, true, false);
    assert(renderer_gl != nullptr);
    assert(renderer_gl->backend_type() == RendererBackend::OpenGL);

    // 3. Vulkan AND OpenGL Fail -> Fallback to Pixman Software Renderer
    auto renderer_pixman = RendererFactory::create_renderer(RendererBackend::Auto, true, true);
    assert(renderer_pixman != nullptr);
    assert(renderer_pixman->backend_type() == RendererBackend::Pixman);

    // 4. String representations
    assert(RendererFactory::backend_from_string("auto") == RendererBackend::Auto);
    assert(RendererFactory::backend_from_string("vulkan") == RendererBackend::Vulkan);
    assert(RendererFactory::backend_from_string("opengl") == RendererBackend::OpenGL);
    assert(RendererFactory::backend_to_string(RendererBackend::Auto) == "Auto");

    std::cout << "[+] smoke_renderer_fallback: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
