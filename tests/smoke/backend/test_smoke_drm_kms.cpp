#include <cassert>
#include <iostream>
#include "comp/backend/drm_backend.hpp"

using namespace tinexus::comp;

int main() {
    std::cout << "[+] Running smoke_drm_kms test suite..." << std::endl;

    DrmBackend backend("/dev/dri/card0");
    assert(backend.type_name() == "DrmBackend");
    assert(backend.initialize() == true);
    assert(backend.is_atomic_supported() == true);

    // Verify DRM Connectors Discovery
    const auto& connectors = backend.connectors();
    assert(connectors.size() == 2);
    assert(connectors[0].name == "HDMI-A-1");
    assert(connectors[0].connected == true);
    assert(connectors[0].width == 1920 && connectors[0].height == 1080);

    // Verify GBM Buffer Allocation
    GbmBuffer buf = backend.allocate_gbm_buffer(1920, 1080);
    assert(buf.fb_id == 2001);
    assert(buf.width == 1920 && buf.height == 1080);
    assert(buf.stride == 7680);
    assert(buf.pixels.size() == 1920 * 1080);
    assert(buf.pixels[0] == 0xFF0000FF); // Solid Blue proof-of-concept frame

    // Verify DRM Atomic Page Flip Execution
    assert(backend.commit_atomic_page_flip(buf) == true);

    backend.poll_events();
    backend.swap_buffers();

    std::cout << "[+] smoke_drm_kms: ALL ASSERTIONS PASSED (100%)" << std::endl;
    return 0;
}
