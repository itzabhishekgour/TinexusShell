#include "common/DisplayUtils.hpp"
#include "common/RuntimePaths.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cstdio>

namespace fs = std::filesystem;

#ifndef TINEXUS_WLROOTS_VERSION
#define TINEXUS_WLROOTS_VERSION "0.19.2"
#endif

namespace tinexus::hardware {

namespace {
std::string resolve_active_renderer() {
    // 1. Check live compositor authoritative state file (dynamic runtime dir or /run/tinexus)
    std::error_code ec;
    std::string dyn_renderer = tinexus::common::RuntimePaths::get_runtime_dir() + "/renderer";
    std::string path_to_check;
    if (fs::exists(dyn_renderer, ec)) {
        path_to_check = dyn_renderer;
    } else if (fs::exists("/run/tinexus/renderer", ec)) {
        path_to_check = "/run/tinexus/renderer";
    }

    if (!path_to_check.empty()) {
        std::ifstream f(path_to_check);
        std::string line;
        if (std::getline(f, line) && !line.empty()) {
            while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) {
                line.pop_back();
            }
            if (line == "vulkan" || line == "vk") return "Vulkan";
            if (line == "gles2" || line == "gl") return "OpenGL";
            if (line == "pixman") return "Pixman";
        }
    }

    // 2. Check environment variable
    const char* env_renderer = std::getenv("WLR_RENDERER");
    if (env_renderer) {
        std::string r = env_renderer;
        if (r == "vulkan" || r == "vk") return "Vulkan";
        if (r == "gles2" || r == "gl") return "OpenGL";
        if (r == "pixman") return "Pixman";
    }

    // 3. If unset, check if DRM card exists (wlroots default on live hardware is OpenGL ES 2)
    if (fs::exists("/dev/dri", ec)) {
        for (const auto& entry : fs::directory_iterator("/dev/dri", ec)) {
            if (entry.path().filename().string().rfind("card", 0) == 0) {
                return "OpenGL";
            }
        }
    }

    return "Pixman";
}
} // namespace

std::string DisplayUtils::get_renderer_backend_string() {
    std::string renderer = resolve_active_renderer();
    return renderer + " Direct Scanout (wlroots " + std::string(TINEXUS_WLROOTS_VERSION) + ")";
}

std::string DisplayUtils::get_compositor_version_string() {
    std::string renderer = resolve_active_renderer();
    std::string tag = (renderer == "OpenGL" || renderer == "Vulkan") ? "Hardware Renderer" : "Renderer";
    return "tinexus-comp (wlroots " + std::string(TINEXUS_WLROOTS_VERSION) + " + " + renderer + " " + tag + ")";
}

DisplayInfo DisplayUtils::get_primary_display(const std::string& base_drm) {
    DisplayInfo info;
    info.connector_name = "eDP-1";
    info.resolution = "1920x1080";
    info.refresh_rate = "Vsync";
    info.connected = false;

    // 1. Primary Authoritative Source: Live active compositor output state
    if (base_drm == "/sys/class/drm") {
        std::error_code ec;
        std::string dyn_display = tinexus::common::RuntimePaths::get_runtime_dir() + "/display";
        std::string display_path;
        if (fs::exists(dyn_display, ec)) {
            display_path = dyn_display;
        } else if (fs::exists("/run/tinexus/display", ec)) {
            display_path = "/run/tinexus/display";
        }

        if (!display_path.empty()) {
            std::ifstream df(display_path);
            std::string line;
            std::string w_str, h_str;
            while (std::getline(df, line)) {
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                while (!v.empty() && (v.back() == '\r' || v.back() == '\n' || v.back() == ' ')) v.pop_back();

                if (k == "connector") info.connector_name = v;
                else if (k == "width") w_str = v;
                else if (k == "height") h_str = v;
                else if (k == "refresh_hz") info.refresh_rate = v + " Hz";
            }
            if (!w_str.empty() && !h_str.empty()) {
                info.resolution = w_str + "x" + h_str;
                info.connected = true;
            }
        }
    }

    // 2. Offline / Standalone Fallback: Discover via DRM/sysfs if compositor state not present
    if (!info.connected && fs::exists(base_drm)) {
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

                        // Parse refresh rate from connector binary EDID if available
                        fs::path edid_path = entry.path() / "edid";
                        if (fs::exists(edid_path)) {
                            std::ifstream ef(edid_path, std::ios::binary);
                            std::vector<uint8_t> edid_bytes((std::istreambuf_iterator<char>(ef)),
                                                             std::istreambuf_iterator<char>());
                            // EDID Detailed Timing Descriptor 1 starts at byte 54
                            if (edid_bytes.size() >= 72) {
                                uint32_t pclk_10khz = static_cast<uint32_t>(edid_bytes[54]) |
                                                      (static_cast<uint32_t>(edid_bytes[55]) << 8);
                                if (pclk_10khz > 0) {
                                    uint32_t ha = static_cast<uint32_t>(edid_bytes[56]) |
                                                  ((static_cast<uint32_t>(edid_bytes[58]) >> 4) << 8);
                                    uint32_t hblank = static_cast<uint32_t>(edid_bytes[57]) |
                                                      ((static_cast<uint32_t>(edid_bytes[58]) & 0x0F) << 8);
                                    uint32_t va = static_cast<uint32_t>(edid_bytes[59]) |
                                                  ((static_cast<uint32_t>(edid_bytes[61]) >> 4) << 8);
                                    uint32_t vblank = static_cast<uint32_t>(edid_bytes[60]) |
                                                      ((static_cast<uint32_t>(edid_bytes[61]) & 0x0F) << 8);
                                    uint32_t htotal = ha + hblank;
                                    uint32_t vtotal = va + vblank;
                                    if (htotal > 0 && vtotal > 0) {
                                        double edid_hz = (static_cast<double>(pclk_10khz) * 10000.0) /
                                                         (static_cast<double>(htotal) * vtotal);
                                        char hz_buf[32];
                                        std::snprintf(hz_buf, sizeof(hz_buf), "%.2f Hz", edid_hz);
                                        info.refresh_rate = hz_buf;
                                    }
                                }
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
