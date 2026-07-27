#include <cassert>
#include <iostream>
#include "comp/surface/surface_manager.hpp"
#include "comp/surface/layer_shell_manager.hpp"
#include "comp/surface/exclusive_zone_calculator.hpp"
#include "comp/window/window_node.hpp"
#include "comp/window/layer_manager.hpp"

using namespace tinexus::comp;

int main() {
    std::cout << "[+] Running smoke_surface_lifecycle test suite..." << std::endl;

    auto& surface_mgr = SurfaceManager::instance();
    auto& layer_shell = LayerShellManager::instance();
    auto& layer_mgr = LayerManager::instance();

    // 1. Surface Creation
    uint32_t s1 = surface_mgr.create_surface(1234, "tinexus-panel");
    assert(s1 > 0);
    SurfaceRecord rec1 = surface_mgr.get_record(s1);
    assert(rec1.role == SurfaceRole::None);
    assert(rec1.lifecycle == SurfaceLifecycle::Created);

    // 2. Assign Role & Immutable Role Rule Verification
    assert(surface_mgr.assign_role(s1, SurfaceRole::LayerSurface) == true);
    rec1 = surface_mgr.get_record(s1);
    assert(rec1.role == SurfaceRole::LayerSurface);
    assert(rec1.lifecycle == SurfaceLifecycle::RoleAssigned);

    // Reassigning different role must fail
    assert(surface_mgr.assign_role(s1, SurfaceRole::XdgToplevel) == false);

    // 3. Layer Surface Creation & Configuration
    uint64_t ls1 = layer_shell.create_layer_surface(s1, LayerType::Top, "panel");
    assert(ls1 >= 1000);

    LayerMargin margin{0, 0, 0, 0};
    uint32_t anchor_top_left_right = static_cast<uint32_t>(LayerAnchor::Top) |
                                     static_cast<uint32_t>(LayerAnchor::Left) |
                                     static_cast<uint32_t>(LayerAnchor::Right);
    assert(layer_shell.configure_layer_surface(ls1, 1920, 48, anchor_top_left_right, 48, margin) == true);

    LayerSurfaceRecord ls_rec = layer_shell.get_layer_surface(ls1);
    assert(ls_rec.configured == true);
    assert(ls_rec.computed_geometry.width == 1920);
    assert(ls_rec.computed_geometry.height == 48);

    rec1 = surface_mgr.get_record(s1);
    assert(rec1.lifecycle == SurfaceLifecycle::Configured);

    // 4. Exercise Full Lifecycle Transitions: Configured -> Mapped -> Visible -> Hidden -> Visible -> Unmapped
    assert(surface_mgr.transition_lifecycle(s1, SurfaceLifecycle::Mapped) == true);
    rec1 = surface_mgr.get_record(s1);
    assert(rec1.lifecycle == SurfaceLifecycle::Mapped);

    assert(surface_mgr.transition_lifecycle(s1, SurfaceLifecycle::Visible) == true);
    rec1 = surface_mgr.get_record(s1);
    assert(rec1.lifecycle == SurfaceLifecycle::Visible);

    assert(surface_mgr.transition_lifecycle(s1, SurfaceLifecycle::Hidden) == true);
    rec1 = surface_mgr.get_record(s1);
    assert(rec1.lifecycle == SurfaceLifecycle::Hidden);

    assert(surface_mgr.transition_lifecycle(s1, SurfaceLifecycle::Visible) == true);
    rec1 = surface_mgr.get_record(s1);
    assert(rec1.lifecycle == SurfaceLifecycle::Visible);

    assert(surface_mgr.transition_lifecycle(s1, SurfaceLifecycle::Unmapped) == true);
    rec1 = surface_mgr.get_record(s1);
    assert(rec1.lifecycle == SurfaceLifecycle::Unmapped);

    // Invalid transition test (Created -> Visible without RoleAssigned should fail)
    uint32_t s_invalid = surface_mgr.create_surface(9999, "invalid-app");
    assert(surface_mgr.transition_lifecycle(s_invalid, SurfaceLifecycle::Visible) == false);

    // 5. Exclusive Zone Layout Calculation
    OutputBounds bounds{1920, 1080};
    assert(bounds.width == 1920);
    UsableArea full_area{0, 0, 1920, 1080};
    UsableArea new_area = ExclusiveZoneCalculator::apply_exclusive_zone(full_area, anchor_top_left_right, 48, ls_rec.computed_geometry);
    assert(new_area.y == 48);
    assert(new_area.height == 1032);

    // 6. LayerManager Ordering Verification (Background < Bottom < Normal < Top < Overlay)
    WindowNode node_bg(1, "wallpaper");
    WindowNode node_normal(2, "app");
    WindowNode node_top(3, "panel");

    layer_mgr.add_node(SceneLayer::Background, &node_bg);
    layer_mgr.add_node(SceneLayer::Normal, &node_normal);
    layer_mgr.add_node(SceneLayer::Top, &node_top);

    assert(layer_mgr.count_layer(SceneLayer::Background) == 1);
    assert(layer_mgr.count_layer(SceneLayer::Normal) == 1);
    assert(layer_mgr.count_layer(SceneLayer::Top) == 1);
    assert(layer_mgr.total_count() == 3);

    auto ordered = layer_mgr.get_ordered_render_nodes();
    assert(ordered.size() == 3);
    assert(ordered[0] == &node_bg);
    assert(ordered[1] == &node_normal);
    assert(ordered[2] == &node_top);

    // 7. Surface Destruction
    assert(layer_shell.destroy_layer_surface(ls1) == true);
    rec1 = surface_mgr.get_record(s1);
    assert(rec1.lifecycle == SurfaceLifecycle::Destroyed);

    std::cout << "[+] smoke_surface_lifecycle: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
