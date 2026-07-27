#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/surface/surface_state.hpp"
#include "comp/surface/buffer_manager.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_buffer_commit");
    tinexus::log::info("Running Buffer Commit Integration Test...");

    tinexus::comp::SurfaceState state(2002, "foot");
    tinexus::comp::BufferManager buf_mgr;

    char dummy_shm[1024];
    assert(buf_mgr.attach_shm_buffer(state, dummy_shm, 800, 600, 3200));
    assert(state.pending_buffer != nullptr);
    assert(state.current_buffer == nullptr);
    assert(!state.mapped);

    buf_mgr.add_damage(state, 0, 0, 800, 600);
    assert(buf_mgr.commit(state));

    assert(state.pending_buffer == nullptr);
    assert(state.current_buffer != nullptr);
    assert(state.mapped);

    tinexus::log::info("[PASS] Buffer Commit Integration Test");
    return 0;
}
