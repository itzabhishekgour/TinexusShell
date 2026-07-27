#ifndef TINEXUS_COMP_LAYER_MANAGER_HPP
#define TINEXUS_COMP_LAYER_MANAGER_HPP

#include <vector>
#include <array>
#include <cstdint>
#include <cstddef>
#include "comp/window/scene_node.hpp"

namespace tinexus::comp {

enum class SceneLayer : size_t {
    Background = 0,
    Bottom = 1,
    Normal = 2,
    Top = 3,
    Overlay = 4,
    Count = 5
};

inline const char* scene_layer_to_string(SceneLayer layer) noexcept {
    switch (layer) {
        case SceneLayer::Background: return "Background";
        case SceneLayer::Bottom: return "Bottom";
        case SceneLayer::Normal: return "Normal";
        case SceneLayer::Top: return "Top";
        case SceneLayer::Overlay: return "Overlay";
        default: return "Unknown";
    }
}

class LayerManager {
public:
    static LayerManager& instance() noexcept;

    LayerManager();
    ~LayerManager() = default;

    void add_node(SceneLayer layer, SceneNode* node);
    void remove_node(SceneNode* node);
    void move_node(SceneNode* node, SceneLayer new_layer);

    void clear() noexcept;

    std::vector<SceneNode*> get_layer_nodes(SceneLayer layer) const;
    std::vector<SceneNode*> get_ordered_render_nodes() const;

    size_t count_layer(SceneLayer layer) const noexcept;
    size_t total_count() const noexcept;

private:
    std::array<std::vector<SceneNode*>, static_cast<size_t>(SceneLayer::Count)> m_layers;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_LAYER_MANAGER_HPP
