#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/window/scene_graph.hpp"
#include "comp/window/window_node.hpp"
#include "comp/renderer/pixman_renderer.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_scene_graph");
    tinexus::log::info("Running Scene Graph Stacking Engine Integration Test...");

    tinexus::comp::SceneGraph scene_graph;

    auto winA = std::make_shared<tinexus::comp::WindowNode>(101, "foot");
    auto winB = std::make_shared<tinexus::comp::WindowNode>(102, "weston-terminal");
    auto winC = std::make_shared<tinexus::comp::WindowNode>(103, "org.gnome.Nautilus");

    // 1. Initial Stacking Order: A -> B -> C
    scene_graph.add_node(winA);
    scene_graph.add_node(winB);
    scene_graph.add_node(winC);

    const auto& stack1 = scene_graph.stacking_order();
    assert(stack1.size() == 3);
    assert(stack1[0]->id() == 101);
    assert(stack1[1]->id() == 102);
    assert(stack1[2]->id() == 103);

    // 2. Raise B to top -> Stacking Order: A -> C -> B
    scene_graph.raise_to_top(102);
    const auto& stack2 = scene_graph.stacking_order();
    assert(stack2[0]->id() == 101);
    assert(stack2[1]->id() == 103);
    assert(stack2[2]->id() == 102);

    // 3. Lower C to bottom -> Stacking Order: C -> A -> B
    scene_graph.lower_to_bottom(103);
    const auto& stack3 = scene_graph.stacking_order();
    assert(stack3[0]->id() == 103);
    assert(stack3[1]->id() == 101);
    assert(stack3[2]->id() == 102);

    // 4. Verify Independent Focus Management
    scene_graph.set_focused_node(101); // Focus A while B remains topmost
    assert(scene_graph.focused_node()->id() == 101);
    assert(stack3.back()->id() == 102); // B is still topmost

    // 5. Verify xdg_toplevel state transitions
    winB->toplevel.set_maximized(true);
    winB->toplevel.set_activated(true);
    assert(winB->toplevel.state().maximized);
    assert(winB->toplevel.state().activated);

    // 6. Verify collection of ordered RenderSurfaces for PixmanRenderer
    auto render_surfaces = scene_graph.collect_render_surfaces();
    assert(render_surfaces.size() == 3);
    assert(render_surfaces[0].id == 103);
    assert(render_surfaces[1].id == 101);
    assert(render_surfaces[2].id == 102);

    // 7. Pass collected render surfaces through PixmanRenderer (Frozen API)
    tinexus::comp::PixmanRenderer renderer;
    assert(renderer.initialize(1920, 1080));
    renderer.begin_frame();
    for (auto& surface : render_surfaces) {
        renderer.compose_surface(surface);
    }
    renderer.end_frame();
    renderer.present();

    tinexus::log::info("Scene Graph Stacking Engine & RenderSurface collection passed 100%!");
    return 0;
}
