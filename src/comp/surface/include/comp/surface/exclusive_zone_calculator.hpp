#ifndef TINEXUS_COMP_EXCLUSIVE_ZONE_CALCULATOR_HPP
#define TINEXUS_COMP_EXCLUSIVE_ZONE_CALCULATOR_HPP

#include <cstdint>

namespace tinexus::comp {

enum class LayerAnchor : uint32_t {
    None = 0,
    Top = 1,
    Bottom = 2,
    Left = 4,
    Right = 8
};

inline LayerAnchor operator|(LayerAnchor a, LayerAnchor b) noexcept {
    return static_cast<LayerAnchor>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(LayerAnchor a, LayerAnchor b) noexcept {
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct LayerMargin {
    int32_t top{0};
    int32_t right{0};
    int32_t bottom{0};
    int32_t left{0};
};

struct LayerGeometry {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};
};

struct OutputBounds {
    int32_t width{0};
    int32_t height{0};
};

struct UsableArea {
    int32_t x{0};
    int32_t y{0};
    int32_t width{0};
    int32_t height{0};
};

class ExclusiveZoneCalculator {
public:
    static LayerGeometry calculate_geometry(
        const OutputBounds& output,
        uint32_t width_req,
        uint32_t height_req,
        uint32_t anchor_flags,
        const LayerMargin& margin);

    static UsableArea apply_exclusive_zone(
        const UsableArea& current_area,
        uint32_t anchor_flags,
        int32_t exclusive_zone,
        const LayerGeometry& geom);
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_EXCLUSIVE_ZONE_CALCULATOR_HPP
