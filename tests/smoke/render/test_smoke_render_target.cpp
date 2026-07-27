#include <cassert>
#include <iostream>
#include "comp/renderer/render_target.hpp"
#include "comp/renderer/gpu_resource_manager.hpp"

using namespace tinexus::comp;

int main() {
    std::cout << "[+] Running smoke_render_target test suite..." << std::endl;

    // Test PixmanTarget
    PixmanTarget pixman_target(1920, 1080);
    assert(pixman_target.width() == 1920 && pixman_target.height() == 1080);
    assert(pixman_target.target_type() == RenderTargetType::Pixman);
    assert(pixman_target.is_valid() == true);
    assert(pixman_target.pixels().size() == 1920 * 1080);

    // Test VulkanSwapchainTarget
    VulkanSwapchainTarget vulkan_target(2560, 1440, 3);
    assert(vulkan_target.width() == 2560 && vulkan_target.height() == 1440);
    assert(vulkan_target.target_type() == RenderTargetType::VulkanSwapchain);
    assert(vulkan_target.is_valid() == true);
    assert(vulkan_target.image_count() == 3);

    // Test EGLSurfaceTarget
    EGLSurfaceTarget egl_target(3840, 2160);
    assert(egl_target.width() == 3840 && egl_target.height() == 2160);
    assert(egl_target.target_type() == RenderTargetType::EGLSurface);
    assert(egl_target.is_valid() == true);

    // Test GpuResourceManager Centralized Memory Lifecycle
    auto& gpu_mgr = GpuResourceManager::instance();
    auto tex = gpu_mgr.create_texture("test_surface_atlas", 1920, 1080);
    assert(tex.id >= 1001);
    assert(tex.name == "test_surface_atlas");
    assert(gpu_mgr.active_resource_count() >= 1);
    assert(gpu_mgr.total_gpu_memory_allocated() > 0);

    assert(gpu_mgr.destroy_texture(tex.id) == true);
    std::cout << "[+] smoke_render_target: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
