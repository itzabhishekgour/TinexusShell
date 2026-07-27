#ifndef TINEXUS_COMP_SCENE_GRAPH_HPP
#define TINEXUS_COMP_SCENE_GRAPH_HPP

#include "comp/window/scene_node.hpp"
#include <vector>
#include <memory>
#include <algorithm>

namespace tinexus::comp {

class SceneGraph {
public:
    SceneGraph() = default;
    ~SceneGraph() = default;

    void add_node(const std::shared_ptr<SceneNode>& node);
    void remove_node(uint64_t id);
    void raise_to_top(uint64_t id);
    void lower_to_bottom(uint64_t id);

    void set_focused_node(uint64_t id);
    [[nodiscard]] std::shared_ptr<SceneNode> focused_node() const noexcept;

    [[nodiscard]] std::vector<RenderSurface> collect_render_surfaces() const;
    [[nodiscard]] const std::vector<std::shared_ptr<SceneNode>>& stacking_order() const noexcept { return m_stack; }

private:
    std::vector<std::shared_ptr<SceneNode>> m_stack;
    uint64_t m_focused_id{0};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SCENE_GRAPH_HPP
