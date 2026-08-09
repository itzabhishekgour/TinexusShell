#ifndef TINEXUS_COMP_WINDOW_NODE_HPP
#define TINEXUS_COMP_WINDOW_NODE_HPP

#include "comp/window/scene_node.hpp"
#include "comp/window/xdg_toplevel_node.hpp"
#include "comp/animation/animation.hpp"

namespace tinexus::comp {

class WindowNode : public SceneNode {
public:
    WindowNode(uint64_t id, const std::string& app_id);
    ~WindowNode() override = default;

    std::vector<RenderSurface> build_render_surfaces() const override;

    int32_t x{0};
    int32_t y{0};
    int32_t width{800};
    int32_t height{600};
    bool visible{true};
    float opacity{0.0f};
    float scale{0.5f};

    SpringAnimation m_opacity_anim{0.0, 1.0, 300.0, 25.0};
    SpringAnimation m_scale_anim{0.5, 1.0, 300.0, 25.0};

    XdgToplevelNode toplevel;
    std::shared_ptr<RenderBuffer> render_buffer;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WINDOW_NODE_HPP
