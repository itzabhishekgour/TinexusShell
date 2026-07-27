#include "comp/window/scene_node.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

SceneNode::SceneNode(uint64_t id, SceneNodeType type)
    : m_id(id), m_type(type) {
    log::info("SceneNode: Allocated SceneNode #{}", m_id);
}

} // namespace tinexus::comp
