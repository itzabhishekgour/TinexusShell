#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/surface/surface_state.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_surface_lifecycle");
    tinexus::log::info("Running Surface Lifecycle Integration Test...");

    tinexus::comp::SurfaceState state(1001, "org.gnome.Terminal");
    assert(state.id == 1001);
    assert(state.app_id == "org.gnome.Terminal");
    assert(!state.mapped);

    state.geometry = {0, 0, 1920, 1080};
    assert(state.geometry.width == 1920);

    tinexus::log::info("[PASS] Surface Lifecycle Integration Test");
    return 0;
}
