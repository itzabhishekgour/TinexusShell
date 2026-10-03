#pragma once

#include <string>
#include <cstdint>

namespace tinexus::hardware {

class BacklightUtils {
public:
    /**
     * @brief Finds the active backlight control device under base_sysfs_backlight.
     * Searches prioritized vendor nodes: intel_backlight, amdgpu_bl*, nvidia_*, acpi_video*, etc.
     */
    static std::string detect_backlight_device(const std::string& base_sysfs_backlight = "/sys/class/backlight");

    /**
     * @brief Gets current screen brightness as a percentage [5 - 100%].
     */
    static int get_brightness_percent(const std::string& base_sysfs_backlight = "/sys/class/backlight");

    /**
     * @brief Sets screen brightness to a percentage [5 - 100%].
     * @param pct Value between 5 and 100
     * @param persist If true, writes new level to hardware.toml
     * @param throttle If true, throttles hardware writes to ~40ms to avoid slider drag stutter
     * @param base_sysfs_backlight Injected sysfs directory path for testing
     */
    static bool set_brightness_percent(int pct,
                                       bool persist = true,
                                       bool throttle = false,
                                       const std::string& base_sysfs_backlight = "/sys/class/backlight");

    /**
     * @brief Steps brightness by delta_pct (+5, -5, etc.).
     */
    static int step_brightness(int delta_pct,
                               bool persist = true,
                               const std::string& base_sysfs_backlight = "/sys/class/backlight");
};

} // namespace tinexus::hardware
