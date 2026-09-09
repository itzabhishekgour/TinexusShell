#pragma once

#include <string>

namespace tinexus::hardware {

struct DisplayInfo {
    std::string connector_name{"eDP-1"};
    std::string resolution{"1920x1080"};
    std::string refresh_rate{"Vsync"};
    std::string formatted_line;
    bool connected{false};
};

class DisplayUtils {
public:
    /**
     * @brief Discovers the primary connected DRM display and its native resolution from /sys/class/drm.
     */
    static DisplayInfo get_primary_display(const std::string& base_drm = "/sys/class/drm");

    /**
     * @brief Returns the dynamically queried compositor and wlroots version string.
     */
    static std::string get_compositor_version_string();

    /**
     * @brief Returns the active renderer display backend string (e.g. "Pixman Direct Scanout (wlroots 0.19.2)").
     */
    static std::string get_renderer_backend_string();
};

} // namespace tinexus::hardware
