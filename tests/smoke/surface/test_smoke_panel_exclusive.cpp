#include <cassert>
#include <iostream>
#include "comp/surface/exclusive_zone_calculator.hpp"
#include "panel/panel_bar.hpp"

using namespace tinexus::comp;
using namespace tinexus::panel;

int main() {
    std::cout << "[+] Running smoke_panel_exclusive test suite..." << std::endl;

    auto& panel = PanelBar::instance();
    uint32_t panel_h = panel.exclusive_height();
    assert(panel_h == 48);

    UsableArea full_screen{0, 0, 1920, 1080};
    uint32_t top_anchor = static_cast<uint32_t>(LayerAnchor::Top) |
                          static_cast<uint32_t>(LayerAnchor::Left) |
                          static_cast<uint32_t>(LayerAnchor::Right);
    LayerGeometry geom{0, 0, 1920, panel_h};
    assert(geom.height == 48);

    UsableArea new_usable = ExclusiveZoneCalculator::apply_exclusive_zone(full_screen, top_anchor, panel_h, geom);

    assert(new_usable.y == 48);
    assert(new_usable.height == 1080 - 48); // 1032
    assert(new_usable.width == 1920);

    std::cout << "[+] smoke_panel_exclusive: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
