#include "monitor/gpu_parser.hpp"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace fs = std::filesystem;

namespace tinexus::monitor {

GpuMetrics GpuParser::parse_gpu(const std::string& base_drm) {
    GpuMetrics gpu;
    gpu.vendor = "Integrated DRM";
    gpu.device_name = "DRM Scanout Graphics";
    gpu.driver = "modesetting";
    gpu.telemetry_status = "Direct Scanout (DRM KMS)";
    gpu.busy_percent = -1.0f;

    if (!fs::exists(base_drm)) {
        return gpu;
    }

    for (const auto& entry : fs::directory_iterator(base_drm)) {
        std::string filename = entry.path().filename().string();
        // Look for card0, card1 (skip connector subdirectories card0-eDP-1, etc.)
        if (filename.rfind("card", 0) == 0 && filename.find('-') == std::string::npos) {
            fs::path vendor_file = entry.path() / "device" / "vendor";
            std::string vendor_hex;
            if (fs::exists(vendor_file)) {
                std::ifstream vf(vendor_file);
                vf >> vendor_hex;
                std::transform(vendor_hex.begin(), vendor_hex.end(), vendor_hex.begin(), ::tolower);
            }

            if (vendor_hex.find("0x1002") != std::string::npos) {
                gpu.vendor = "AMD";
                gpu.driver = "amdgpu";
                gpu.device_name = "Radeon Graphics";
                // Real AMD GPU busy percent from sysfs
                fs::path busy_file = entry.path() / "device" / "gpu_busy_percent";
                if (fs::exists(busy_file)) {
                    std::ifstream bf(busy_file);
                    int busy = 0;
                    if (bf >> busy) {
                        gpu.busy_percent = static_cast<float>(std::clamp(busy, 0, 100));
                        gpu.telemetry_status = "Utilization: " + std::to_string(busy) + "% (amdgpu sysfs)";
                    }
                } else {
                    gpu.telemetry_status = "amdgpu DRM KMS";
                }
                break;
            } else if (vendor_hex.find("0x8086") != std::string::npos) {
                gpu.vendor = "Intel";
                gpu.driver = "i915 / Xe";
                gpu.device_name = "Intel Graphics";
                // Real Intel GPU frequency from sysfs
                fs::path cur_freq = entry.path() / "gt_act_freq_mhz";
                fs::path min_freq = entry.path() / "gt_min_freq_mhz";
                fs::path max_freq = entry.path() / "gt_max_freq_mhz";
                if (fs::exists(cur_freq) && fs::exists(max_freq)) {
                    std::ifstream cf(cur_freq);
                    std::ifstream mf(max_freq);
                    int cur = 0, max = 0, min = 0;
                    if (fs::exists(min_freq)) {
                        std::ifstream min_f(min_freq);
                        min_f >> min;
                    }
                    if ((cf >> cur) && (mf >> max) && max > min) {
                        float load = (static_cast<float>(cur - min) / static_cast<float>(max - min)) * 100.0f;
                        gpu.busy_percent = std::clamp(load, 0.0f, 100.0f);
                        gpu.telemetry_status = "Clock: " + std::to_string(cur) + " MHz (Idle: " + std::to_string(min) + " MHz, Max: " + std::to_string(max) + " MHz)";
                    } else if ((cf >> cur) && (mf >> max) && max > 0) {
                        gpu.busy_percent = (static_cast<float>(cur) / static_cast<float>(max)) * 100.0f;
                        gpu.telemetry_status = "Clock: " + std::to_string(cur) + " / " + std::to_string(max) + " MHz";
                    }
                } else {
                    gpu.telemetry_status = "i915 Direct DRM Scanout";
                }
                break;
            } else if (vendor_hex.find("0x10de") != std::string::npos) {
                gpu.vendor = "NVIDIA";
                gpu.driver = "nouveau / nvidia";
                gpu.device_name = "GeForce Graphics";
                gpu.telemetry_status = "Telemetry requires proprietary NVML driver";
                gpu.busy_percent = -1.0f;
                break;
            }
        }
    }

    return gpu;
}

} // namespace tinexus::monitor
