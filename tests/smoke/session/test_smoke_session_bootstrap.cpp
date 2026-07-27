#include <cassert>
#include <iostream>
#include <cstdlib>
#include "session/session_manager.hpp"
#include "session/env_bootstrap.hpp"

using namespace tinexus::session;

int main() {
    std::cout << "[+] Running smoke_session_bootstrap test suite..." << std::endl;

    auto& sm = SessionManager::instance();
    assert(sm.state() == SessionState::Stopped);

    sm.transition_state(SessionState::Starting);
    assert(sm.state() == SessionState::Starting);

    sm.transition_state(SessionState::Authenticating);
    sm.transition_state(SessionState::Launching);
    sm.transition_state(SessionState::Running);
    assert(sm.state() == SessionState::Running);

    assert(EnvBootstrap::apply_environment() == true);
    const char* session_type = std::getenv("XDG_SESSION_TYPE");
    const char* current_desktop = std::getenv("XDG_CURRENT_DESKTOP");

    assert(session_type != nullptr && std::string(session_type) == "wayland");
    assert(current_desktop != nullptr && std::string(current_desktop) == "Tinexus");

    std::cout << "[+] smoke_session_bootstrap: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
