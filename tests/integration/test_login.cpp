#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "login/pam_authenticator.hpp"
#include "login/session_launcher.hpp"
#include "login/login_model.hpp"
#include "login/login_view.hpp"
#include "login/login_controller.hpp"

void test_pam_authenticator_valid_and_invalid() {
    tinexus::login::PamAuthenticator pam;
    assert(pam.authenticate("user", "password") == true);
    assert(pam.authenticate("user", "") == false);
    assert(pam.authenticate("", "password") == false);
    std::cout << "[PASS] test_pam_authenticator_valid_and_invalid\n";
}

void test_session_launcher_environment() {
    bool launched = tinexus::login::SessionLauncher::launch_session("testuser", 1001, 1001);
    assert(launched);
    assert(std::string(std::getenv("XDG_CURRENT_DESKTOP")) == "Tinexus");
    assert(std::string(std::getenv("XDG_SESSION_TYPE")) == "wayland");
    std::cout << "[PASS] test_session_launcher_environment\n";
}

void test_login_controller_state_machine() {
    tinexus::login::LoginController controller;
    bool success = controller.submit_credentials("tinexus-user", "secretpass");
    assert(success);
    assert(controller.model().state() == tinexus::login::LoginState::SessionHandedOff);
    std::cout << "[PASS] test_login_controller_state_machine\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_login");
    tinexus::log::info("Running Integration Test Suite for Tinexus Login Manager...");

    test_pam_authenticator_valid_and_invalid();
    test_session_launcher_environment();
    test_login_controller_state_machine();

    tinexus::log::info("All Tinexus Login Manager integration tests passed 100%!");
    return 0;
}
