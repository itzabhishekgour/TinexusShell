#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/server/server.hpp"
#include "comp/server/global_registry.hpp"
#include "comp/server/surface_tree.hpp"
#include "comp/backend/backend.hpp"
#include "comp/renderer/renderer.hpp"

void test_global_registry_advertising() {
    tinexus::comp::GlobalRegistry registry;
    assert(registry.has_global("wl_compositor"));
    assert(registry.has_global("wl_shm"));
    assert(registry.has_global("wl_seat"));
    assert(registry.has_global("xdg_wm_base"));
    assert(registry.has_global("data_device_manager"));
    std::cout << "[PASS] test_global_registry_advertising\n";
}

void test_surface_tree_and_window_ids() {
    tinexus::comp::SurfaceTree tree;
    auto id1 = tree.create_surface("org.gnome.Terminal", "Terminal");
    auto id2 = tree.create_surface("firefox", "Mozilla Firefox");

    assert(id1 > 0);
    assert(id2 > id1);
    assert(tree.active_surface_count() == 2);

    assert(tree.destroy_surface(id1));
    assert(tree.active_surface_count() == 1);
    std::cout << "[PASS] test_surface_tree_and_window_ids\n";
}

void test_wayland_compositor_server_session() {
    tinexus::comp::TinexusServer server;
    assert(server.initialize());
    assert(server.wayland_display() == "wayland-0");

    server.run();
    server.stop();
    std::cout << "[PASS] test_wayland_compositor_server_session\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_wayland_session");
    tinexus::log::info("Running Integration Test Suite for Milestone v0.2 Wayland Compositor Session...");

    test_global_registry_advertising();
    test_surface_tree_and_window_ids();
    test_wayland_compositor_server_session();

    tinexus::log::info("All Wayland Compositor Session integration tests passed 100%!");
    return 0;
}
