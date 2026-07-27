#include <cassert>
#include <iostream>
#include "session/session_manager.hpp"

using namespace tinexus::session;

int main() {
    std::cout << "[+] Running smoke_multiple_sessions test suite..." << std::endl;

    SessionManager sm1;
    sm1.transition_state(SessionState::Running);
    assert(sm1.state() == SessionState::Running);

    SessionManager sm2;
    sm2.transition_state(SessionState::Authenticating);
    assert(sm2.state() == SessionState::Authenticating);

    sm1.transition_state(SessionState::Stopped);
    assert(sm1.state() == SessionState::Stopped);

    std::cout << "[+] smoke_multiple_sessions: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
