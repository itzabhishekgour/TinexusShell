#ifndef TINEXUS_COMP_PIXMAN_RENDERER_HPP
#define TINEXUS_COMP_PIXMAN_RENDERER_HPP

#include "comp/renderer/renderer.hpp"
#include "comp/renderer/renderer_factory.hpp"
#include <vector>

namespace tinexus::comp {

class PixmanRenderer : public Renderer {
public:
    PixmanRenderer() = default;
    ~PixmanRenderer() override;

    bool initialize(uint32_t width, uint32_t height) override;
    void begin_frame() override;
    void compose_surface(RenderSurface& surface) override;
    void damage_region(const DamageRegion& region) override;
    void end_frame() override;
    void present() override;
    [[nodiscard]] RendererBackend backend_type() const noexcept override;

    [[nodiscard]] const std::vector<uint32_t>& canvas_buffer() const noexcept { return m_canvas; }
    [[nodiscard]] const DamageRegion& active_damage() const noexcept { return m_active_damage; }
    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }

private:
    uint32_t m_width{0};
    uint32_t m_height{0};
    std::vector<uint32_t> m_canvas; // ARGB8888 canvas buffer
    DamageRegion m_active_damage;
    bool m_in_frame{false};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_PIXMAN_RENDERER_HPP
