#include "comp/surface/layer_shell_manager.hpp"
#include "comp/surface/surface_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

LayerShellManager& LayerShellManager::instance() noexcept {
    static LayerShellManager s_instance;
    return s_instance;
}

uint64_t LayerShellManager::create_layer_surface(uint32_t surface_id, LayerType layer, const std::string& namespace_id) {
    uint64_t id = m_next_id++;
    LayerSurfaceRecord rec;
    rec.id = id;
    rec.surface_id = surface_id;
    rec.layer = layer;
    rec.namespace_id = namespace_id;

    // Assign SurfaceRole::LayerSurface in SurfaceManager
    SurfaceManager::instance().assign_role(surface_id, SurfaceRole::LayerSurface);

    m_surfaces[id] = rec;

    log::info("LayerShellManager: Created layer surface #{} (surface={}, layer={}, ns='{}')",
              id, surface_id, layer_type_to_string(layer), namespace_id);

    return id;
}

bool LayerShellManager::configure_layer_surface(uint64_t id, uint32_t width, uint32_t height, uint32_t anchor_flags, int32_t exclusive_zone, const LayerMargin& margin) {
    auto it = m_surfaces.find(id);
    if (it == m_surfaces.end()) return false;

    auto& rec = it->second;
    rec.width_req = width;
    rec.height_req = height;
    rec.anchor_flags = anchor_flags;
    rec.exclusive_zone = exclusive_zone;
    rec.margin = margin;

    OutputBounds output_bounds{1920, 1080};
    rec.computed_geometry = ExclusiveZoneCalculator::calculate_geometry(output_bounds, width, height, anchor_flags, margin);
    rec.configured = true;

    SurfaceManager::instance().transition_lifecycle(rec.surface_id, SurfaceLifecycle::Configured);

    log::info("LayerShellManager: Configured layer surface #{} geom=({},{},{},{}) exclusive={}",
              id, rec.computed_geometry.x, rec.computed_geometry.y, rec.computed_geometry.width, rec.computed_geometry.height, exclusive_zone);

    return true;
}

bool LayerShellManager::destroy_layer_surface(uint64_t id) {
    auto it = m_surfaces.find(id);
    if (it == m_surfaces.end()) return false;

    uint32_t surface_id = it->second.surface_id;
    SurfaceManager::instance().transition_lifecycle(surface_id, SurfaceLifecycle::Destroyed);

    m_surfaces.erase(it);

    log::info("LayerShellManager: Destroyed layer surface #{}", id);
    return true;
}

LayerSurfaceRecord LayerShellManager::get_layer_surface(uint64_t id) const {
    auto it = m_surfaces.find(id);
    if (it != m_surfaces.end()) return it->second;
    return LayerSurfaceRecord{};
}

std::vector<LayerSurfaceRecord> LayerShellManager::get_surfaces_for_layer(LayerType layer) const {
    std::vector<LayerSurfaceRecord> result;
    for (const auto& [id, rec] : m_surfaces) {
        if (rec.layer == layer) {
            result.push_back(rec);
        }
    }
    return result;
}

std::vector<LayerSurfaceRecord> LayerShellManager::get_all_layer_surfaces() const {
    std::vector<LayerSurfaceRecord> result;
    for (const auto& [id, rec] : m_surfaces) {
        result.push_back(rec);
    }
    return result;
}

} // namespace tinexus::comp
