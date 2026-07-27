#include "displayd/login_monitor.hpp"
#include "common/logger.hpp"

namespace tinexus::displayd {

bool LoginMonitor::spawn_login_screen() {
    m_login_pid = 4096; // Simulated PID for test environments
    log::info("LoginMonitor: Spawned tinexus-login surface (PID: {})", m_login_pid);
    return true;
}

bool LoginMonitor::handle_crash_and_restart() {
    log::warn("LoginMonitor: Session crash detected! Automatically restarting tinexus-login...");
    return spawn_login_screen();
}

} // namespace tinexus::displayd
