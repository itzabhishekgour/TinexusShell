#ifndef TINEXUS_COMP_RENDERER_VULKAN_RENDERER_HPP
#define TINEXUS_COMP_RENDERER_VULKAN_RENDERER_HPP

#include "comp/renderer/renderer.hpp"
#include "comp/renderer/renderer_factory.hpp"
#include "comp/renderer/render_target.hpp"
#include "comp/renderer/gpu_resource_manager.hpp"

#include "comp/renderer/blur_pass.hpp"

namespace tinexus::comp {

class VulkanRenderer : public Renderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() override = default;

    bool initialize(uint32_t width, uint32_t height) override;
    bool initialize() { return initialize(1920, 1080); }
    void begin_frame() override;
    void compose_surface(RenderSurface& surface) override;
    void damage_region(const DamageRegion& region) override;
    void end_frame() override;
    void present() override;
    [[nodiscard]] RendererBackend backend_type() const noexcept override { return RendererBackend::Vulkan; }

    [[nodiscard]] bool is_gpu_accelerated() const noexcept { return m_vulkan_initialized; }
    bool render_target(IRenderTarget& target, const DamageRegion& damage);

private:
    bool m_vulkan_initialized{false};
    GpuResourceHandle m_active_pipeline;
    BlurPass m_blur_pass;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_VULKAN_RENDERER_HPP
