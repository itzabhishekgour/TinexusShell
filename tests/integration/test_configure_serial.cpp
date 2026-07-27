#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "comp/surface/configure_serial.hpp"

int main() {
    tinexus::log::set_component_name("integration_test_configure_serial");
    tinexus::log::info("Running Configure Serial Manager Integration Test...");

    tinexus::comp::ConfigureSerialManager mgr;
    uint32_t s1 = mgr.generate();
    uint32_t s2 = mgr.generate();

    assert(s1 > 0);
    assert(s2 > s1);
    assert(mgr.pending_count() == 2);

    assert(mgr.validate(s1));
    assert(mgr.pending_count() == 1);

    assert(!mgr.validate(999999)); // Invalid token rejection
    assert(mgr.validate(s2));
    assert(mgr.pending_count() == 0);

    tinexus::log::info("[PASS] Configure Serial Manager Integration Test");
    return 0;
}
