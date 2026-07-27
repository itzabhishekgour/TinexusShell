#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/input/seat_manager.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_pointer_focus");
    tinexus::log::info("Running Pointer Focus Dispatcher Integration Test...");

    tinexus::comp::SeatManager seat;
    assert(seat.bind_seat("seat0"));

    seat.send_pointer_enter(2002, 100, 200);
    assert(seat.focused_surface_id() == 2002);

    seat.send_pointer_motion(150, 250);
    seat.send_button_click(272, 1); // Left mouse click

    seat.send_pointer_leave(2002);
    assert(seat.focused_surface_id() == 0);

    tinexus::log::info("Pointer Focus Dispatcher test passed 100%!");
    return 0;
}
