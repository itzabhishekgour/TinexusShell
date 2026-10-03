#include "common/BacklightUtils.hpp"
#include "common/HardwareConfig.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

namespace tinexus::hardware {

namespace {
static int s_cached_brightness = -1;
static std::chrono::steady_clock::time_point s_last_write_time{};

void run_process_async(const char* bin, const char* arg1, const char* arg2) {
    if (!fs::exists(bin)) return;
    pid_t pid = fork();
    if (pid == 0) {
        pid_t p2 = fork();
        if (p2 == 0) {
            execl(bin, bin, arg1, arg2, nullptr);
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        waitpid(pid, nullptr, 0);
    }
}
} // namespace

std::string BacklightUtils::detect_backlight_device(const std::string& base_sysfs_backlight) {
    std::error_code ec;
    if (!fs::exists(base_sysfs_backlight, ec)) {
        return "";
    }

    std::string best_device;
    int best_priority = -1;

    for (const auto& entry : fs::directory_iterator(base_sysfs_backlight, ec)) {
        if (!entry.is_directory() && !entry.is_symlink()) continue;
        std::string name = entry.path().filename().string();
        int prio = 0;
        if (name.find("intel") != std::string::npos) prio = 10;
        else if (name.find("amdgpu") != std::string::npos) prio = 9;
        else if (name.find("nvidia") != std::string::npos) prio = 8;
        else if (name.find("acpi") != std::string::npos) prio = 5;
        else prio = 1;

        if (prio > best_priority) {
            best_priority = prio;
            best_device = entry.path().string();
        }
    }

    return best_device;
}

int BacklightUtils::get_brightness_percent(const std::string& base_sysfs_backlight) {
    std::string dev_path = detect_backlight_device(base_sysfs_backlight);
    if (!dev_path.empty()) {
        std::ifstream cur_file(fs::path(dev_path) / "brightness");
        std::ifstream max_file(fs::path(dev_path) / "max_brightness");

        long cur = 0, max = 0;
        if ((cur_file >> cur) && (max_file >> max) && max > 0) {
            int pct = static_cast<int>((cur * 100) / max);
            pct = std::clamp(pct, 5, 100);
            s_cached_brightness = pct;
            return pct;
        }
    }

    if (s_cached_brightness > 0) {
        return s_cached_brightness;
    }

    auto cfg = HardwareConfig::load();
    s_cached_brightness = std::clamp(cfg.brightness, 5, 100);
    return s_cached_brightness;
}

bool BacklightUtils::set_brightness_percent(int pct,
                                            bool persist,
                                            bool throttle,
                                            const std::string& base_sysfs_backlight) {
    pct = std::clamp(pct, 5, 100);
    s_cached_brightness = pct;

    if (throttle) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_write_time).count();
        if (elapsed < 40) {
            // Coalesce rapid drag updates
            return true;
        }
        s_last_write_time = now;
    } else {
        s_last_write_time = std::chrono::steady_clock::now();
    }

    bool success = false;
    std::string dev_path = detect_backlight_device(base_sysfs_backlight);
    if (!dev_path.empty()) {
        std::ifstream max_file(fs::path(dev_path) / "max_brightness");
        long max_raw = 0;
        if (max_file >> max_raw && max_raw > 0) {
            long target_raw = (pct * max_raw) / 100;
            std::ofstream cur_file(fs::path(dev_path) / "brightness");
            if (cur_file.is_open()) {
                cur_file << target_raw << std::endl;
                cur_file.flush();
                success = cur_file.good();
            }
        }
    }

    // Fallback via brightnessctl if sysfs direct write was not successful
    if (!success && fs::exists("/usr/bin/brightnessctl")) {
        std::string arg = std::to_string(pct) + "%";
        run_process_async("/usr/bin/brightnessctl", "s", arg.c_str());
        success = true;
    }

    if (persist) {
        auto cfg = HardwareConfig::load();
        cfg.brightness = pct;
        HardwareConfig::save(cfg);
    }

    return success;
}

int BacklightUtils::step_brightness(int delta_pct,
                                    bool persist,
                                    const std::string& base_sysfs_backlight) {
    int cur = get_brightness_percent(base_sysfs_backlight);
    int target = std::clamp(cur + delta_pct, 5, 100);
    set_brightness_percent(target, persist, false, base_sysfs_backlight);
    return target;
}

} // namespace tinexus::hardware
