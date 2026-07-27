#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/surface/surface_state.hpp"
#include "comp/surface/resource_cleanup.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_resource_cleanup");
    tinexus::log::info("Running Resource Cleanup Integration Test...");

    tinexus::comp::SurfaceState state(3003, "weston-terminal");
    state.mapped = true;

    assert(tinexus::comp::ResourceCleanup::destroy_surface_resources(state));
    assert(!state.mapped);
    assert(state.pending_buffer == nullptr);
    assert(state.current_buffer == nullptr);

    tinexus::log::info("[PASS] Resource Cleanup Integration Test");
    return 0;
}
