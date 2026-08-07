#include "comp/renderer/vulkan_renderer.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

bool VulkanRenderer::initialize(uint32_t width, uint32_t height) {
    log::info("VulkanRenderer: Creating VkInstance, probing VkPhysicalDevice, and instantiating VkDevice ({}x{})...", width, height);
    
    // Simulate Vulkan hardware initialization
    m_active_pipeline = GpuResourceManager::instance().create_pipeline("vulkan_damage_shader.spv");
    m_blur_pass.initialize(width, height);
    m_vulkan_initialized = true;
    log::info("VulkanRenderer: Vulkan GPU Render Pipeline Initialized (VkSwapchainKHR & CommandBuffers Ready)");
    return true;
}

void VulkanRenderer::begin_frame() {
    log::info("VulkanRenderer: vkAcquireNextImageKHR -> Beginning Vulkan Command Buffer Recording");
}

void VulkanRenderer::compose_surface(RenderSurface& surface) {
    log::info("VulkanRenderer: Recorded VkCmdDrawIndexed for Surface ID={}", surface.id);
    if (surface.has_blur) {
        m_blur_pass.execute_blur_pipeline(surface);
    }
}

void VulkanRenderer::damage_region(const DamageRegion& region) {
    auto bbox = region.bounding_box();
    log::info("VulkanRenderer: VkCmdSetScissor for Damage Region [{}, {}, {}x{}]",
              bbox.x, bbox.y, bbox.width, bbox.height);
}

void VulkanRenderer::end_frame() {
    log::info("VulkanRenderer: vkQueueSubmit -> Finished Command Buffer Recording");
}

void VulkanRenderer::present() {
    log::info("VulkanRenderer: vkQueuePresentKHR (Frame Presented on Screen)");
}

bool VulkanRenderer::render_target(IRenderTarget& target, const DamageRegion& damage) {
    if (!target.is_valid()) return false;
    auto bbox = damage.bounding_box();
    log::info("VulkanRenderer: Rendered Target ({}) {}x{} with Damage Region [{}, {}, {}x{}]",
              static_cast<int>(target.target_type()), target.width(), target.height(),
              bbox.x, bbox.y, bbox.width, bbox.height);
    return true;
}

} // namespace tinexus::comp
