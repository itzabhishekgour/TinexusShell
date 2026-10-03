#include "common/GraphicsProbe.hpp"
#include <cassert>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace tinexus::hardware;

namespace {

struct TempDirGuard {
    fs::path path;
    explicit TempDirGuard(std::string name) {
        path = fs::temp_directory_path() / ("tx_test_probe_" + name + "_" + std::to_string(::getpid()));
        fs::remove_all(path);
        fs::create_directories(path);
    }
    ~TempDirGuard() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};

void write_file(const fs::path& p, const std::string& content) {
    fs::create_directories(p.parent_path());
    std::ofstream ofs(p);
    ofs << content;
}

} // namespace

void test_mock_intel_gpu_detection() {
    std::cout << "[TEST 1] Testing Intel i915 GPU enumeration..." << std::endl;
    TempDirGuard mock("intel_only");

    // Setup mock Intel card0
    write_file(mock.path / "card0" / "device" / "vendor", "0x8086\n");
    write_file(mock.path / "card0-eDP-1" / "status", "connected\n");

    // Create mock driver directory and symlink
    fs::path drivers_dir = mock.path / "drivers" / "i915";
    fs::create_directories(drivers_dir);
    fs::create_directories(mock.path / "card0" / "device");
    std::error_code ec;
    fs::create_symlink(drivers_dir, mock.path / "card0" / "device" / "driver", ec);

    auto gpus = GraphicsProbe::enumerate_gpus(mock.path.string());
    assert(gpus.size() == 1);
    assert(gpus[0].is_intel == true);
    assert(gpus[0].is_nvidia == false);
    assert(gpus[0].card_name == "card0");
    assert(gpus[0].card_path == "/dev/dri/card0");
    assert(gpus[0].display_connected == true);
    assert(gpus[0].connector_name == "eDP-1");
    assert(gpus[0].driver_name == "i915");

    std::cout << "  -> PASSED: Intel i915 properly detected and parsed." << std::endl;
}

void test_mock_hybrid_nvidia_first_selection() {
    std::cout << "[TEST 2] Testing hybrid dual-GPU where NVIDIA is card0 and Intel is card1..." << std::endl;
    TempDirGuard mock("hybrid_nvidia_first");

    // Mock NVIDIA as card0
    write_file(mock.path / "card0" / "device" / "vendor", "0x10de\n");
    fs::path nouveau_dir = mock.path / "drivers" / "nouveau";
    fs::create_directories(nouveau_dir);
    std::error_code ec;
    fs::create_symlink(nouveau_dir, mock.path / "card0" / "device" / "driver", ec);

    // Mock Intel as card1 with internal display eDP-1 connected
    write_file(mock.path / "card1" / "device" / "vendor", "0x8086\n");
    write_file(mock.path / "card1-eDP-1" / "status", "connected\n");
    fs::path i915_dir = mock.path / "drivers" / "i915";
    fs::create_directories(i915_dir);
    fs::create_symlink(i915_dir, mock.path / "card1" / "device" / "driver", ec);

    auto gpus = GraphicsProbe::enumerate_gpus(mock.path.string());
    assert(gpus.size() == 2);

    // Verify ordering and identities
    bool found_intel = false;
    bool found_nvidia = false;
    for (const auto& g : gpus) {
        if (g.card_name == "card0") {
            assert(g.is_nvidia == true);
            assert(g.is_intel == false);
            found_nvidia = true;
        } else if (g.card_name == "card1") {
            assert(g.is_intel == true);
            assert(g.is_nvidia == false);
            assert(g.display_connected == true);
            assert(g.connector_name == "eDP-1");
            found_intel = true;
        }
    }
    assert(found_intel && found_nvidia);

    // Evaluate: NVIDIA should be ignored as secondary, Intel card1 chosen as candidate
    auto res = GraphicsProbe::evaluate(mock.path.string());
    assert(res.device.card_name == "card1");
    assert(res.device.card_path == "/dev/dri/card1");
    // Since /dev/dri/card1 is not a live kernel node in this test environment,
    // probe fails closed gracefully without crash:
    assert(res.decision == RendererDecision::Pixman);
    assert(res.selected_renderer == "pixman");
    assert(res.selected_card_node.empty()); // MUST be empty on probe fail!
    assert(res.hardware_available == false);

    std::cout << "  -> PASSED: NVIDIA card0 ignored, Intel card1 evaluated, graceful fallback executed." << std::endl;
}

void test_mock_pure_nvidia_selection() {
    std::cout << "[TEST 3] Testing pure NVIDIA system (no Intel iGPU) with connected display..." << std::endl;
    TempDirGuard mock("pure_nvidia");

    // Pure NVIDIA system: card0 is NVIDIA with active HDMI-A-1 display
    write_file(mock.path / "card0" / "device" / "vendor", "0x10de\n");
    write_file(mock.path / "card0-HDMI-A-1" / "status", "connected\n");
    fs::path nouveau_dir = mock.path / "drivers" / "nouveau";
    fs::create_directories(nouveau_dir);
    std::error_code ec;
    fs::create_symlink(nouveau_dir, mock.path / "card0" / "device" / "driver", ec);

    auto res = GraphicsProbe::evaluate(mock.path.string());
    assert(res.device.card_name == "card0");
    assert(res.device.is_nvidia == true);
    assert(res.device.display_connected == true);
    // Since mock /dev/dri/card0 does not exist, probe safely fails without crashing:
    assert(res.hardware_available == false);
    assert(!res.reason.empty());

    std::cout << "  -> PASSED: Pure NVIDIA GPU with active display evaluated as candidate (not blindly skipped)." << std::endl;
}

void test_mock_disconnected_display_fallback() {
    std::cout << "[TEST 4] Testing GPU without connected display fallback..." << std::endl;
    TempDirGuard mock("no_display");

    write_file(mock.path / "card0" / "device" / "vendor", "0x8086\n");
    write_file(mock.path / "card0-HDMI-A-1" / "status", "disconnected\n");

    auto res = GraphicsProbe::evaluate(mock.path.string());
    assert(res.decision == RendererDecision::Pixman);
    assert(res.selected_renderer == "pixman");
    assert(res.selected_card_node.empty());
    assert(res.hardware_available == false);
    assert(res.reason.find("no active connected display") != std::string::npos);

    std::cout << "  -> PASSED: Disconnected display cleanly triggers fallback." << std::endl;
}

void test_mock_missing_drm_sysfs() {
    std::cout << "[TEST 5] Testing completely missing /sys/class/drm directory..." << std::endl;
    auto res = GraphicsProbe::evaluate("/tmp/tinexus_nonexistent_drm_path_9999");
    assert(res.decision == RendererDecision::Pixman);
    assert(res.selected_renderer == "pixman");
    assert(res.selected_card_node.empty());
    assert(res.hardware_available == false);

    std::cout << "  -> PASSED: Missing DRM directory cleanly degrades without crashing." << std::endl;
}

void test_mock_missing_card_node_safety() {
    std::cout << "[TEST 6] Testing missing card node safety guard (headless / container edge case)..." << std::endl;
    TempDirGuard mock("missing_card");

    // Mock GPU detected in sysfs with connected display, but card_path doesn't exist in /dev/dri
    write_file(mock.path / "card99" / "device" / "vendor", "0x8086\n");
    write_file(mock.path / "card99-eDP-1" / "status", "connected\n");

    auto res = GraphicsProbe::evaluate(mock.path.string());
    assert(res.hardware_available == false);
    assert(res.selected_card_node.empty());
    assert(res.reason.find("Primary DRM card node does not exist") != std::string::npos);

    std::cout << "  -> PASSED: Missing card node safely handled without crash." << std::endl;
}

void test_probe_egl_gbm_invalid_node() {
    std::cout << "[TEST 7] Testing probe_egl_gbm() on invalid node..." << std::endl;
    std::string ver, err;
    bool ok = GraphicsProbe::probe_egl_gbm("/dev/null", ver, err);
    assert(!ok);
    assert(!err.empty());

    bool ok_empty = GraphicsProbe::probe_egl_gbm("", ver, err);
    assert(!ok_empty);

    std::cout << "  -> PASSED: probe_egl_gbm() fails safely with error string on invalid node." << std::endl;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "  Tinexus GraphicsProbe Test & Failure-Injection   " << std::endl;
    std::cout << "==================================================" << std::endl;

    test_mock_intel_gpu_detection();
    test_mock_hybrid_nvidia_first_selection();
    test_mock_pure_nvidia_selection();
    test_mock_disconnected_display_fallback();
    test_mock_missing_drm_sysfs();
    test_mock_missing_card_node_safety();
    test_probe_egl_gbm_invalid_node();

    std::cout << "==================================================" << std::endl;
    std::cout << "  ALL GRAPHICS PROBE TESTS PASSED (100% OK)       " << std::endl;
    std::cout << "==================================================" << std::endl;
    return 0;
}
