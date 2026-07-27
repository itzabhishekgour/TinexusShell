#ifndef TINEXUS_COMP_WINDOW_NODE_HPP
#define TINEXUS_COMP_WINDOW_NODE_HPP

#include "comp/window/scene_node.hpp"
#include "comp/window/xdg_toplevel_node.hpp"

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
    float opacity{1.0f};

    XdgToplevelNode toplevel;
    std::shared_ptr<RenderBuffer> render_buffer;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WINDOW_NODE_HPP
