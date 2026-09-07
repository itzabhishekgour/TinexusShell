#include "common/DisplayUtils.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>

namespace fs = std::filesystem;

#ifndef TINEXUS_WLROOTS_VERSION
#define TINEXUS_WLROOTS_VERSION "0.19.2"
#endif

namespace tinexus::hardware {

std::string DisplayUtils::get_renderer_backend_string() {
    const char* env_renderer = std::getenv("WLR_RENDERER");
    std::string renderer = "Pixman";
    if (env_renderer) {
        std::string r = env_renderer;
        if (r == "vulkan" || r == "vk") renderer = "Vulkan";
        else if (r == "gles2" || r == "gl") renderer = "OpenGL";
        else if (r == "pixman") renderer = "Pixman";
    }
    return renderer + " Direct Scanout (wlroots " + std::string(TINEXUS_WLROOTS_VERSION) + ")";
}

std::string DisplayUtils::get_compositor_version_string() {
    const char* env_renderer = std::getenv("WLR_RENDERER");
    std::string renderer = "Pixman";
    if (env_renderer) {
        std::string r = env_renderer;
        if (r == "vulkan" || r == "vk") renderer = "Vulkan";
        else if (r == "gles2" || r == "gl") renderer = "OpenGL";
        else if (r == "pixman") renderer = "Pixman";
    }
    return "tinexus-comp (wlroots " + std::string(TINEXUS_WLROOTS_VERSION) + " + " + renderer + " Renderer)";
}

DisplayInfo DisplayUtils::get_primary_display(const std::string& base_drm) {
    DisplayInfo info;
    info.connector_name = "eDP-1";
    info.resolution = "1920x1080";
    info.refresh_rate = "60.00 Hz";
    info.connected = false;

    if (fs::exists(base_drm)) {
        for (const auto& entry : fs::directory_iterator(base_drm)) {
            std::string name = entry.path().filename().string();
            // Match cardX-ConnectorName (e.g. card1-eDP-1, card0-Virtual-1, card0-HDMI-A-1)
            if (name.find("card") != std::string::npos && name.find('-') != std::string::npos) {
                fs::path status_path = entry.path() / "status";
                if (fs::exists(status_path)) {
                    std::ifstream sf(status_path);
                    std::string status;
                    if (sf >> status && status == "connected") {
                        auto dash = name.find('-');
                        info.connector_name = name.substr(dash + 1);
                        info.connected = true;

                        fs::path modes_path = entry.path() / "modes";
                        if (fs::exists(modes_path)) {
                            std::ifstream mf(modes_path);
                            std::string first_mode;
                            if (mf >> first_mode && !first_mode.empty()) {
                                info.resolution = first_mode;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    // Format human-readable resolution: replace 'x' with ' × '
    std::string res_formatted = info.resolution;
    auto x_pos = res_formatted.find('x');
    if (x_pos != std::string::npos) {
        res_formatted.replace(x_pos, 1, " × ");
    }

    info.formatted_line = res_formatted + " @ " + info.refresh_rate + "  •  " + get_renderer_backend_string();
    return info;
}

} // namespace tinexus::hardware
