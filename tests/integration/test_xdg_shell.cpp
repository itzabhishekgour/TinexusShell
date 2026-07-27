#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/surface/xdg_shell_manager.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_xdg_shell");
    tinexus::log::info("Running XDG Shell Protocol Binding Integration Test...");

    tinexus::comp::XdgShellManager shell_mgr;
    assert(shell_mgr.bind_xdg_wm_base(1));

    uint64_t surface_id = shell_mgr.create_xdg_surface(8008);
    assert(surface_id == 8008);
    assert(shell_mgr.active_xdg_surfaces_count() == 1);

    uint32_t serial = shell_mgr.send_toplevel_configure(8008, 1280, 720);
    assert(serial > 0);

    assert(shell_mgr.handle_ack_configure(8008, serial));
    assert(!shell_mgr.handle_ack_configure(8008, 999999)); // Reject invalid serial

    tinexus::log::info("XDG Shell Protocol Binding test passed 100%!");
    return 0;
}
