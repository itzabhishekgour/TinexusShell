#include "comp/surface/exclusive_zone_calculator.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::comp {

LayerGeometry ExclusiveZoneCalculator::calculate_geometry(
    const OutputBounds& output,
    uint32_t width_req,
    uint32_t height_req,
    uint32_t anchor_flags,
    const LayerMargin& margin) {

    LayerGeometry geom;
    bool anchor_top = (anchor_flags & static_cast<uint32_t>(LayerAnchor::Top)) != 0;
    bool anchor_bottom = (anchor_flags & static_cast<uint32_t>(LayerAnchor::Bottom)) != 0;
    bool anchor_left = (anchor_flags & static_cast<uint32_t>(LayerAnchor::Left)) != 0;
    bool anchor_right = (anchor_flags & static_cast<uint32_t>(LayerAnchor::Right)) != 0;

    // Horizontal placement
    if (anchor_left && anchor_right) {
        geom.x = margin.left;
        geom.width = std::max(0, output.width - margin.left - margin.right);
    } else if (anchor_left) {
        geom.x = margin.left;
        geom.width = static_cast<int32_t>(width_req);
    } else if (anchor_right) {
        geom.x = output.width - margin.right - static_cast<int32_t>(width_req);
        geom.width = static_cast<int32_t>(width_req);
    } else {
        geom.x = (output.width - static_cast<int32_t>(width_req)) / 2;
        geom.width = static_cast<int32_t>(width_req);
    }

    // Vertical placement
    if (anchor_top && anchor_bottom) {
        geom.y = margin.top;
        geom.height = std::max(0, output.height - margin.top - margin.bottom);
    } else if (anchor_top) {
        geom.y = margin.top;
        geom.height = static_cast<int32_t>(height_req);
    } else if (anchor_bottom) {
        geom.y = output.height - margin.bottom - static_cast<int32_t>(height_req);
        geom.height = static_cast<int32_t>(height_req);
    } else {
        geom.y = (output.height - static_cast<int32_t>(height_req)) / 2;
        geom.height = static_cast<int32_t>(height_req);
    }

    return geom;
}

UsableArea ExclusiveZoneCalculator::apply_exclusive_zone(
    const UsableArea& current_area,
    uint32_t anchor_flags,
    int32_t exclusive_zone,
    const LayerGeometry& geom) {

    if (exclusive_zone <= 0) return current_area;

    UsableArea area = current_area;
    bool anchor_top = (anchor_flags & static_cast<uint32_t>(LayerAnchor::Top)) != 0;
    bool anchor_bottom = (anchor_flags & static_cast<uint32_t>(LayerAnchor::Bottom)) != 0;
    bool anchor_left = (anchor_flags & static_cast<uint32_t>(LayerAnchor::Left)) != 0;
    bool anchor_right = (anchor_flags & static_cast<uint32_t>(LayerAnchor::Right)) != 0;

    int consumed = exclusive_zone;

    if (anchor_top && !anchor_bottom) {
        area.y += consumed;
        area.height -= consumed;
    } else if (anchor_bottom && !anchor_top) {
        area.height -= consumed;
    } else if (anchor_left && !anchor_right) {
        area.x += consumed;
        area.width -= consumed;
    } else if (anchor_right && !anchor_left) {
        area.width -= consumed;
    }

    area.width = std::max(0, area.width);
    area.height = std::max(0, area.height);

    log::info("ExclusiveZone applied: zone={} new_usable_area=({},{},{},{})",
              exclusive_zone, area.x, area.y, area.width, area.height);

    return area;
}

} // namespace tinexus::comp
