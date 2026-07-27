#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/input/seat_manager.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_input_seat");
    tinexus::log::info("Running Input Seat Subsystem Integration Test...");

    tinexus::comp::SeatManager seat;
    assert(seat.bind_seat("seat0"));
    assert(seat.seat_bound());

    seat.keymap().update_modifiers(1, 0, 0, 0);
    assert(seat.keymap().state().mods_depressed == 1);

    tinexus::log::info("Input Seat Subsystem test passed 100%!");
    return 0;
}
