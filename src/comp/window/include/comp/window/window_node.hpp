#ifndef TINEXUS_COMP_WINDOW_NODE_HPP
#define TINEXUS_COMP_WINDOW_NODE_HPP

#include "comp/window/scene_node.hpp"
#include "comp/window/xdg_toplevel_node.hpp"
#include "comp/animation/animation.hpp"
#include <memory>

namespace tinexus::comp {

enum class AnimationPhase : uint8_t {
    None,
    Minimizing,
    Restoring
};

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

    // Minimize/restore state
    int32_t saved_x{0}, saved_y{0};
    uint32_t saved_w{800}, saved_h{600};
    int32_t dock_icon_x{0}, dock_icon_y{0};
    bool minimized{false};

    AnimationPhase animation_phase{AnimationPhase::None};
    std::unique_ptr<BaseAnimation> active_dock_anim;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WINDOW_NODE_HPP
