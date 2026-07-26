#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/surface/surface_manager.hpp"

void test_surface_state_transitions() {
    auto& manager = tinexus::comp::SurfaceManager::instance();

    uint32_t surface_id = manager.create_surface(4050, "org.mozilla.firefox");
    assert(surface_id > 0);

    auto record = manager.get_record(surface_id);
    assert(record.state == tinexus::comp::SurfaceState::Created);
    assert(record.app_id == "org.mozilla.firefox");

    assert(manager.transition_state(surface_id, tinexus::comp::SurfaceState::Configured));
    assert(manager.get_record(surface_id).state == tinexus::comp::SurfaceState::Configured);

    assert(manager.transition_state(surface_id, tinexus::comp::SurfaceState::Mapped));
    assert(manager.get_record(surface_id).state == tinexus::comp::SurfaceState::Mapped);

    assert(manager.transition_state(surface_id, tinexus::comp::SurfaceState::Focused));
    assert(manager.get_record(surface_id).state == tinexus::comp::SurfaceState::Focused);

    assert(manager.transition_state(surface_id, tinexus::comp::SurfaceState::Minimized));
    assert(manager.get_record(surface_id).state == tinexus::comp::SurfaceState::Minimized);

    assert(manager.transition_state(surface_id, tinexus::comp::SurfaceState::Destroyed));
    assert(manager.get_record(surface_id).state == tinexus::comp::SurfaceState::Destroyed);

    std::cout << "[PASS] test_surface_state_transitions\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_wayland");
    tinexus::log::info("Running integration test suite for Wayland Server Subsystem...");

    test_surface_state_transitions();

    tinexus::log::info("All Wayland Server integration tests passed successfully!");
    return 0;
}
