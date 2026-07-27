#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/input/seat_manager.hpp"
#include "comp/focus/focus_manager.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_keyboard_focus");
    tinexus::log::info("Running Keyboard Focus Dispatcher Integration Test...");

    tinexus::comp::SeatManager seat;
    assert(seat.bind_seat("seat0"));

    seat.send_keyboard_enter(1001);
    assert(seat.focused_surface_id() == 1001);

    seat.send_key_event(30, 1); // Key press 'a'
    seat.send_keyboard_leave(1001);
    assert(seat.focused_surface_id() == 0);

    tinexus::log::info("Keyboard Focus Dispatcher test passed 100%!");
    return 0;
}
