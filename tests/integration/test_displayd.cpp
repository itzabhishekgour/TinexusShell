#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "displayd/vt_manager.hpp"
#include "displayd/seat_manager.hpp"
#include "displayd/login_monitor.hpp"
#include "displayd/session_launcher.hpp"
#include "displayd/systemd_interface.hpp"
#include "displayd/display_daemon.hpp"

void test_vt_allocation_and_switch() {
    tinexus::displayd::VtManager vt;
    assert(vt.allocate_vt(7));
    assert(vt.active_vt() == 7);
    assert(vt.switch_vt(1));
    assert(vt.active_vt() == 1);
    std::cout << "[PASS] test_vt_allocation_and_switch\n";
}

void test_seat_manager() {
    tinexus::displayd::SeatManager seat;
    assert(seat.acquire_seat("seat0"));
    assert(seat.is_seat_acquired());
    assert(seat.release_seat());
    assert(!seat.is_seat_acquired());
    std::cout << "[PASS] test_seat_manager\n";
}

void test_login_monitor_and_crash_recovery() {
    tinexus::displayd::LoginMonitor login;
    assert(login.spawn_login_screen());
    assert(login.login_pid() > 0);
    assert(login.handle_crash_and_restart());
    std::cout << "[PASS] test_login_monitor_and_crash_recovery\n";
}

void test_systemd_interface_and_daemon_bootstrap() {
    assert(tinexus::displayd::SystemdInterface::notify_ready());
    assert(tinexus::displayd::SystemdInterface::notify_stopping());

    tinexus::displayd::DisplayDaemon daemon;
    assert(daemon.bootstrap());
    assert(daemon.shutdown());
    std::cout << "[PASS] test_systemd_interface_and_daemon_bootstrap\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_displayd");
    tinexus::log::info("Running Integration Test Suite for Tinexus Display Manager...");

    test_vt_allocation_and_switch();
    test_seat_manager();
    test_login_monitor_and_crash_recovery();
    test_systemd_interface_and_daemon_bootstrap();

    tinexus::log::info("All Tinexus Display Manager integration tests passed 100%!");
    return 0;
}
