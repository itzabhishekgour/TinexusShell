#ifndef TINEXUS_COMP_SCENE_NODE_HPP
#define TINEXUS_COMP_SCENE_NODE_HPP

#include "comp/renderer/render_surface.hpp"
#include <vector>
#include <cstdint>
#include <memory>

namespace tinexus::comp {

enum class SceneNodeType {
    Window,
    Popup,
    Panel,
    Cursor,
    LayerShell
};

class SceneNode {
public:
    explicit SceneNode(uint64_t id, SceneNodeType type = SceneNodeType::Window);
    virtual ~SceneNode() = default;

    [[nodiscard]] uint64_t id() const noexcept { return m_id; }
    [[nodiscard]] SceneNodeType type() const noexcept { return m_type; }

    virtual std::vector<RenderSurface> build_render_surfaces() const = 0;

protected:
    uint64_t m_id{0};
    SceneNodeType m_type{SceneNodeType::Window};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SCENE_NODE_HPP
