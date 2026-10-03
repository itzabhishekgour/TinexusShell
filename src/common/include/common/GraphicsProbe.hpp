#pragma once

#include <string>
#include <vector>

namespace tinexus::hardware {

enum class RendererDecision {
    HardwareGles2,
    Pixman
};

struct GpuDeviceInfo {
    std::string card_name;        // e.g. "card0"
    std::string card_path;        // e.g. "/dev/dri/card0"
    std::string render_path;      // e.g. "/dev/dri/renderD128"
    std::string vendor_id;        // e.g. "0x8086"
    std::string driver_name;      // e.g. "i915"
    std::string connector_name;   // e.g. "eDP-1"
    bool display_connected{false};
    bool is_intel{false};
    bool is_nvidia{false};
    bool is_amd{false};
};

struct GraphicsProbeResult {
    RendererDecision decision{RendererDecision::Pixman};
    std::string selected_renderer{"pixman"};
    std::string selected_card_node; // Set ONLY if Intel is verified with connected display; otherwise empty
    std::string reason;
    std::string egl_version;
    GpuDeviceInfo device;
    bool hardware_available{false};
};

class GraphicsProbe {
public:
    /**
     * @brief Discovers and profiles all available DRM GPU devices under sysfs without fixed node assumptions.
     * @param base_drm Path to DRM sysfs directory (defaults to "/sys/class/drm").
     * @return List of discovered GPU devices with vendor and display connectivity profiles.
     */
    static std::vector<GpuDeviceInfo> enumerate_gpus(const std::string& base_drm = "/sys/class/drm");

    /**
     * @brief Probes a DRM device node for GBM device creation and EGL GLES2 context initialization.
     *        Strictly RAII-managed: all file descriptors, GBM objects, and EGL resources are released on exit.
     * @param node_path Path to DRM render or card node (e.g. "/dev/dri/renderD128").
     * @param out_egl_version Populated with initialized EGL version string (e.g. "1.5").
     * @param out_error Populated with failure reason if probe returns false.
     * @return True if hardware acceleration is verified functional, false otherwise.
     */
    static bool probe_egl_gbm(const std::string& node_path, std::string& out_egl_version, std::string& out_error);

    /**
     * @brief Executes the full conservative graphics capability decision pipeline.
     *        1. Enumerates DRM devices.
     *        2. Identifies Intel i915 GPU (ignoring NVIDIA 0x10DE).
     *        3. Verifies connected display output.
     *        4. Probes GBM/EGL/GLES2 hardware stack.
     *        5. Fails closed to Pixman if ANY step fails.
     * @param base_drm Path to DRM sysfs directory.
     * @return Authoritative graphics probe result.
     */
    static GraphicsProbeResult evaluate(const std::string& base_drm = "/sys/class/drm");
};

} // namespace tinexus::hardware
