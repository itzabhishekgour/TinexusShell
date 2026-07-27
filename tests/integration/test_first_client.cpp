#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/server/server.hpp"
#include "comp/surface/surface_state.hpp"
#include "comp/surface/configure_serial.hpp"
#include "comp/surface/buffer_manager.hpp"
#include "comp/surface/frame_callback.hpp"
#include "comp/surface/resource_cleanup.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_first_client");
    tinexus::log::info("Running First Client End-to-End Surface Transaction Protocol Test...");

    // 1. Initialize Compositor Display Server
    tinexus::comp::TinexusServer server;
    assert(server.initialize());
    assert(!server.wayland_display().empty());

    // 2. Allocate SurfaceState for Client Window (e.g. weston-terminal / foot)
    tinexus::comp::SurfaceState client_surface(5005, "weston-terminal");
    assert(client_surface.id == 5005);
    assert(!client_surface.mapped);

    // 3. Generate & Validate Configure Serial
    tinexus::comp::ConfigureSerialManager serial_mgr;
    uint32_t serial = serial_mgr.generate();
    assert(serial_mgr.validate(serial));
    client_surface.configured = true;
    client_surface.last_configure_serial = serial;

    // 4. Attach SHM Buffer, Record Damage, & Commit
    tinexus::comp::BufferManager buf_mgr;
    char client_shm_buffer[4096];
    assert(buf_mgr.attach_shm_buffer(client_surface, client_shm_buffer, 1024, 768, 4096));
    buf_mgr.add_damage(client_surface, 0, 0, 1024, 768);
    assert(buf_mgr.commit(client_surface));
    assert(client_surface.mapped);
    assert(client_surface.current_buffer != nullptr);

    // 5. Dispatch Frame Callbacks
    tinexus::comp::FrameCallbackManager cb_mgr;
    cb_mgr.request_callback(client_surface.id, 101);
    assert(cb_mgr.send_frame_done_all(20500) == 1);

    // 6. Clean Resource Cleanup
    assert(tinexus::comp::ResourceCleanup::destroy_surface_resources(client_surface));
    assert(!client_surface.mapped);

    tinexus::log::info("First Client End-to-End Protocol Transaction completed 100%!");
    return 0;
}
