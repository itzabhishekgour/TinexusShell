#include <cassert>
#include <iostream>
#include "comp/window/layer_manager.hpp"
#include "comp/window/window_node.hpp"

using namespace tinexus::comp;

int main() {
    std::cout << "[+] Running smoke_notification_overlay test suite..." << std::endl;

    auto& layer_mgr = LayerManager::instance();

    WindowNode bg_node(10, "wallpaper");
    WindowNode app_node(20, "foot");
    WindowNode panel_node(30, "panel");
    WindowNode notif_node(40, "notification");

    layer_mgr.add_node(SceneLayer::Background, &bg_node);
    layer_mgr.add_node(SceneLayer::Normal, &app_node);
    layer_mgr.add_node(SceneLayer::Top, &panel_node);
    layer_mgr.add_node(SceneLayer::Overlay, &notif_node);

    auto ordered = layer_mgr.get_ordered_render_nodes();
    assert(ordered.size() == 4);

    // Verify strict Z-order stacking: Background < Normal < Top < Overlay (Notification)
    assert(ordered[0] == &bg_node);
    assert(ordered[1] == &app_node);
    assert(ordered[2] == &panel_node);
    assert(ordered[3] == &notif_node);

    std::cout << "[+] smoke_notification_overlay: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
