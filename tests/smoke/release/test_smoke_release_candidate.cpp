#include <cassert>
#include <iostream>
#include <fstream>
#include "comp/backend/drm_backend.hpp"
#include "session/session_manager.hpp"

using namespace tinexus::comp;
using namespace tinexus::session;

int main() {
    std::cout << "[+] Running smoke_release_candidate test suite..." << std::endl;

    // 1. Validate GPU Driver Fallback Ladder
    DrmBackend drm_intel("/dev/dri/card0");
    assert(drm_intel.initialize());
    assert(drm_intel.gpu_vendor() == GpuVendor::Intel);

    DrmBackend drm_amd("/dev/dri/card1");
    assert(drm_amd.initialize());
    assert(drm_amd.gpu_vendor() == GpuVendor::Amd);

    // 2. Validate Session Supervisor Execution
    auto& session = SessionManager::instance();
    assert(session.state() == SessionState::Stopped);
    assert(session.start_session(true));
    assert(session.state() == SessionState::Running);
    assert(session.stop_session());
    assert(session.state() == SessionState::Stopped);

    std::cout << "[+] smoke_release_candidate: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
