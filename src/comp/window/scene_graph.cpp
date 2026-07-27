#include "comp/window/scene_graph.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

void SceneGraph::add_node(const std::shared_ptr<SceneNode>& node) {
    if (!node) return;
    remove_node(node->id());
    m_stack.push_back(node);
    log::info("SceneGraph: Added node #{} to scene graph (stack size: {})", node->id(), m_stack.size());
}

void SceneGraph::remove_node(uint64_t id) {
    auto it = std::remove_if(m_stack.begin(), m_stack.end(),
        [id](const std::shared_ptr<SceneNode>& node) { return node && node->id() == id; });
    if (it != m_stack.end()) {
        m_stack.erase(it, m_stack.end());
        log::info("SceneGraph: Removed node #{}", id);
    }
    if (m_focused_id == id) {
        m_focused_id = m_stack.empty() ? 0 : m_stack.back()->id();
    }
}

void SceneGraph::raise_to_top(uint64_t id) {
    auto it = std::find_if(m_stack.begin(), m_stack.end(),
        [id](const std::shared_ptr<SceneNode>& node) { return node && node->id() == id; });
    if (it != m_stack.end()) {
        auto node = *it;
        m_stack.erase(it);
        m_stack.push_back(node);
        log::info("SceneGraph: Raised node #{} to top of stacking order", id);
    }
}

void SceneGraph::lower_to_bottom(uint64_t id) {
    auto it = std::find_if(m_stack.begin(), m_stack.end(),
        [id](const std::shared_ptr<SceneNode>& node) { return node && node->id() == id; });
    if (it != m_stack.end()) {
        auto node = *it;
        m_stack.erase(it);
        m_stack.insert(m_stack.begin(), node);
        log::info("SceneGraph: Lowered node #{} to bottom of stacking order", id);
    }
}

void SceneGraph::set_focused_node(uint64_t id) {
    m_focused_id = id;
    log::info("SceneGraph: Set active focus to node #{}", id);
}

std::shared_ptr<SceneNode> SceneGraph::focused_node() const noexcept {
    auto it = std::find_if(m_stack.begin(), m_stack.end(),
        [this](const std::shared_ptr<SceneNode>& node) { return node && node->id() == m_focused_id; });
    return (it != m_stack.end()) ? *it : nullptr;
}

std::vector<RenderSurface> SceneGraph::collect_render_surfaces() const {
    std::vector<RenderSurface> surfaces;
    for (const auto& node : m_stack) {
        if (!node) continue;
        auto node_surfaces = node->build_render_surfaces();
        surfaces.insert(surfaces.end(), node_surfaces.begin(), node_surfaces.end());
    }
    return surfaces;
}

} // namespace tinexus::comp
