#ifndef TINEXUS_COMP_LAYER_SHELL_MANAGER_HPP
#define TINEXUS_COMP_LAYER_SHELL_MANAGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include "comp/surface/exclusive_zone_calculator.hpp"

namespace tinexus::comp {

enum class LayerType : uint32_t {
    Background = 0,
    Bottom = 1,
    Top = 2,
    Overlay = 3
};

inline const char* layer_type_to_string(LayerType layer) noexcept {
    switch (layer) {
        case LayerType::Background: return "Background";
        case LayerType::Bottom: return "Bottom";
        case LayerType::Top: return "Top";
        case LayerType::Overlay: return "Overlay";
        default: return "Unknown";
    }
}

struct LayerSurfaceRecord {
    uint64_t id{0};
    uint32_t surface_id{0};
    LayerType layer{LayerType::Background};
    std::string namespace_id;
    uint32_t width_req{0};
    uint32_t height_req{0};
    uint32_t anchor_flags{0};
    int32_t exclusive_zone{0};
    LayerMargin margin;
    LayerGeometry computed_geometry;
    bool configured{false};
};

class LayerShellManager {
public:
    static LayerShellManager& instance() noexcept;

    LayerShellManager() = default;
    ~LayerShellManager() = default;

    uint64_t create_layer_surface(uint32_t surface_id, LayerType layer, const std::string& namespace_id);
    bool configure_layer_surface(uint64_t id, uint32_t width, uint32_t height, uint32_t anchor_flags, int32_t exclusive_zone, const LayerMargin& margin);
    bool destroy_layer_surface(uint64_t id);

    LayerSurfaceRecord get_layer_surface(uint64_t id) const;
    std::vector<LayerSurfaceRecord> get_surfaces_for_layer(LayerType layer) const;
    std::vector<LayerSurfaceRecord> get_all_layer_surfaces() const;
    size_t count() const noexcept { return m_surfaces.size(); }

private:
    uint64_t m_next_id{1000};
    std::unordered_map<uint64_t, LayerSurfaceRecord> m_surfaces;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_LAYER_SHELL_MANAGER_HPP
