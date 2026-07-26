#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "session/session_manager.hpp"
#include "session/env_bootstrap.hpp"
#include "session/autostart_parser.hpp"

void test_session_environment_bootstrap() {
    auto& env = tinexus::session::EnvironmentBootstrapper::instance();
    assert(env.bootstrap_environment());

    auto map = env.get_environment_map();
    assert(map["WAYLAND_DISPLAY"] == "wayland-0");
    assert(map["XDG_CURRENT_DESKTOP"] == "Tinexus");
    assert(map["XDG_SESSION_TYPE"] == "wayland");

    std::cout << "[PASS] test_session_environment_bootstrap\n";
}

void test_session_autostart_parsing() {
    auto& autostart = tinexus::session::AutostartParser::instance();
    auto entries = autostart.parse_autostart_directory("/etc/xdg/autostart");
    assert(!entries.empty());

    size_t launched = autostart.launch_autostart_apps(entries);
    assert(launched >= 1);

    std::cout << "[PASS] test_session_autostart_parsing\n";
}

void test_session_lifecycle_transitions() {
    auto& session = tinexus::session::SessionManager::instance();
    assert(session.state() == tinexus::session::SessionState::Booting);

    assert(session.start_session(true)); // Dry-run start
    assert(session.state() == tinexus::session::SessionState::EnvironmentReady);

    assert(session.stop_session());
    assert(session.state() == tinexus::session::SessionState::Stopped);

    std::cout << "[PASS] test_session_lifecycle_transitions\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_session");
    tinexus::log::info("Running Integration Test Suite for Session Manager (tinexus-session)...");

    test_session_environment_bootstrap();
    test_session_autostart_parsing();
    test_session_lifecycle_transitions();

    tinexus::log::info("All Session Manager integration tests passed 100%!");
    return 0;
}
