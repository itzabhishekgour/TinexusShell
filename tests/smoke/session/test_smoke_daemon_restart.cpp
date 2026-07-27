#include <cassert>
#include <iostream>
#include "session/session_manager.hpp"

using namespace tinexus::session;

int main() {
    std::cout << "[+] Running smoke_daemon_restart test suite..." << std::endl;

    auto& sm = SessionManager::instance();
    sm.register_daemon("tinexus-panel", 4001);
    assert(sm.daemons().at("tinexus-panel").pid == 4001);

    // Simulate panel crash and supervisor recovery
    assert(sm.handle_daemon_crash("tinexus-panel") == true);
    assert(sm.daemons().at("tinexus-panel").restart_count == 1);
    assert(sm.daemons().at("tinexus-panel").pid == 9001);

    std::cout << "[+] smoke_daemon_restart: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
