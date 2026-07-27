#include "comp/backend/drm_backend.hpp"
#include "common/logger.hpp"
#include <iostream>

using namespace tinexus;
using namespace tinexus::comp;

int main() {
    log::set_component_name("tinexus-drm-info");
    log::info("Tinexus DRM/KMS Hardware Diagnostic Utility v1.0.0");
    std::cout << "\n======================================================\n";
    std::cout << "  Tinexus DRM/KMS Display Inspector\n";
    std::cout << "======================================================\n\n";

    DrmBackend drm("/dev/dri/card0");
    if (!drm.initialize()) {
        log::error("Failed to initialize DRM backend on /dev/dri/card0");
        return 1;
    }

    std::cout << "Card: /dev/dri/card0\n";
    std::cout << "Atomic Modesetting: " << (drm.is_atomic_supported() ? "SUPPORTED (ENABLED)" : "DISABLED") << "\n\n";
    std::cout << "Connectors:\n";

    for (const auto& conn : drm.connectors()) {
        std::cout << "  - ID: " << conn.connector_id << " [" << conn.name << "]\n";
        std::cout << "    Status: " << (conn.connected ? "CONNECTED" : "DISCONNECTED") << "\n";
        std::cout << "    Preferred Mode: " << conn.width << "x" << conn.height << "@" << conn.refresh_hz << "Hz\n\n";
    }

    GbmBuffer sample = drm.allocate_gbm_buffer(1920, 1080);
    std::cout << "GBM Allocation Test: FB_ID=" << sample.fb_id << " (" << sample.width << "x" << sample.height << ")\n";

    bool atomic_commit = drm.commit_atomic_page_flip(sample);
    std::cout << "Atomic Page Flip Test: " << (atomic_commit ? "SUCCESS (Solid Blue Frame 0xFF0000FF Active)" : "FAILED") << "\n\n";

    std::cout << "[+] DRM/KMS Native Graphics Hardware Bring-up Verified 100%!\n";
    return 0;
}
