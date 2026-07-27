#include "comp/window/layer_manager.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::comp {

LayerManager& LayerManager::instance() noexcept {
    static LayerManager s_instance;
    return s_instance;
}

LayerManager::LayerManager() {
    clear();
}

void LayerManager::clear() noexcept {
    for (auto& vec : m_layers) {
        vec.clear();
    }
}

void LayerManager::add_node(SceneLayer layer, SceneNode* node) {
    if (!node) return;
    remove_node(node); // Ensure no duplicates
    size_t idx = static_cast<size_t>(layer);
    if (idx < m_layers.size()) {
        m_layers[idx].push_back(node);
        log::info("LayerManager: Added node #{} to layer {}", node->id(), scene_layer_to_string(layer));
    }
}

void LayerManager::remove_node(SceneNode* node) {
    if (!node) return;
    for (auto& vec : m_layers) {
        auto it = std::find(vec.begin(), vec.end(), node);
        if (it != vec.end()) {
            vec.erase(it);
        }
    }
}

void LayerManager::move_node(SceneNode* node, SceneLayer new_layer) {
    if (!node) return;
    remove_node(node);
    add_node(new_layer, node);
}

std::vector<SceneNode*> LayerManager::get_layer_nodes(SceneLayer layer) const {
    size_t idx = static_cast<size_t>(layer);
    if (idx < m_layers.size()) {
        return m_layers[idx];
    }
    return {};
}

std::vector<SceneNode*> LayerManager::get_ordered_render_nodes() const {
    std::vector<SceneNode*> result;
    for (size_t i = 0; i < m_layers.size(); ++i) {
        for (SceneNode* node : m_layers[i]) {
            if (node) {
                result.push_back(node);
            }
        }
    }
    return result;
}

size_t LayerManager::count_layer(SceneLayer layer) const noexcept {
    size_t idx = static_cast<size_t>(layer);
    if (idx < m_layers.size()) {
        return m_layers[idx].size();
    }
    return 0;
}

size_t LayerManager::total_count() const noexcept {
    size_t total = 0;
    for (const auto& vec : m_layers) {
        total += vec.size();
    }
    return total;
}

} // namespace tinexus::comp
