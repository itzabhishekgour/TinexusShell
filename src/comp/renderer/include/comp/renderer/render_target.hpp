#ifndef TINEXUS_COMP_RENDERER_RENDER_TARGET_HPP
#define TINEXUS_COMP_RENDERER_RENDER_TARGET_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace tinexus::comp {

enum class RenderTargetType {
    Pixman,
    VulkanSwapchain,
    EGLSurface
};

class IRenderTarget {
public:
    virtual ~IRenderTarget() = default;

    [[nodiscard]] virtual uint32_t width() const noexcept = 0;
    [[nodiscard]] virtual uint32_t height() const noexcept = 0;
    [[nodiscard]] virtual RenderTargetType target_type() const noexcept = 0;
    [[nodiscard]] virtual bool is_valid() const noexcept = 0;
};

class PixmanTarget : public IRenderTarget {
public:
    PixmanTarget(uint32_t w, uint32_t h);
    ~PixmanTarget() override = default;

    [[nodiscard]] uint32_t width() const noexcept override { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept override { return m_height; }
    [[nodiscard]] RenderTargetType target_type() const noexcept override { return RenderTargetType::Pixman; }
    [[nodiscard]] bool is_valid() const noexcept override { return !m_pixels.empty(); }

    [[nodiscard]] std::vector<uint32_t>& pixels() noexcept { return m_pixels; }

private:
    uint32_t m_width{1920};
    uint32_t m_height{1080};
    std::vector<uint32_t> m_pixels;
};

class VulkanSwapchainTarget : public IRenderTarget {
public:
    VulkanSwapchainTarget(uint32_t w, uint32_t h, uint32_t swapchain_image_count = 3);
    ~VulkanSwapchainTarget() override = default;

    [[nodiscard]] uint32_t width() const noexcept override { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept override { return m_height; }
    [[nodiscard]] RenderTargetType target_type() const noexcept override { return RenderTargetType::VulkanSwapchain; }
    [[nodiscard]] bool is_valid() const noexcept override { return m_swapchain_count > 0; }
    [[nodiscard]] uint32_t image_count() const noexcept { return m_swapchain_count; }

private:
    uint32_t m_width{1920};
    uint32_t m_height{1080};
    uint32_t m_swapchain_count{3};
};

class EGLSurfaceTarget : public IRenderTarget {
public:
    EGLSurfaceTarget(uint32_t w, uint32_t h);
    ~EGLSurfaceTarget() override = default;

    [[nodiscard]] uint32_t width() const noexcept override { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept override { return m_height; }
    [[nodiscard]] RenderTargetType target_type() const noexcept override { return RenderTargetType::EGLSurface; }
    [[nodiscard]] bool is_valid() const noexcept override { return m_valid; }

private:
    uint32_t m_width{1920};
    uint32_t m_height{1080};
    bool m_valid{true};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_RENDER_TARGET_HPP
