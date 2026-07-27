#include "comp/renderer/render_target.hpp"

namespace tinexus::comp {

PixmanTarget::PixmanTarget(uint32_t w, uint32_t h)
    : m_width(w), m_height(h) {
    m_pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h), 0x00000000);
}

VulkanSwapchainTarget::VulkanSwapchainTarget(uint32_t w, uint32_t h, uint32_t swapchain_image_count)
    : m_width(w), m_height(h), m_swapchain_count(swapchain_image_count) {}

EGLSurfaceTarget::EGLSurfaceTarget(uint32_t w, uint32_t h)
    : m_width(w), m_height(h) {}

} // namespace tinexus::comp
