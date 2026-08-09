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
    surface.scale = scale;
    surface.buffer = render_buffer;

    // Apply backdrop blur based on application type (macOS / Windows glassmorphism)
    const std::string& app = toplevel.app_id();
    if (app == "launcher" || app == "tinexus-launcher") {
        surface.has_blur = true;
        surface.blur_radius = 40;
        surface.blur_tint = 0x13131ACC; // 80% opacity dark surface
    } else if (app == "notifications" || app == "tinexus-notif") {
        surface.has_blur = true;
        surface.blur_radius = 30;
        surface.blur_tint = 0x13131ACC;
    } else if (app == "lock" || app == "tinexus-lock") {
        surface.has_blur = true;
        surface.blur_radius = 60;
        surface.blur_tint = 0x0A0A0EB3; // 70% opacity dark tint
    }

    return {surface};
}

} // namespace tinexus::comp
