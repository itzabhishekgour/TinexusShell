#include "comp/window/window_node.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

WindowNode::WindowNode(uint64_t id, const std::string& app_id)
    : SceneNode(id, SceneNodeType::Window) {
    toplevel.set_app_id(app_id);
    log::info("WindowNode: Created window node #{} ({})", id, app_id);
}

std::vector<RenderSurface> WindowNode::build_render_surfaces() const {
    if (!visible) return {};

    RenderSurface surface(m_id);
    surface.x = x;
    surface.y = y;
    surface.opacity = opacity;
    surface.buffer = render_buffer;

    return {surface};
}

} // namespace tinexus::comp
