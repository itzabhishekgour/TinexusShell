#include "monitor/power_parser.hpp"
#include <filesystem>
#include <fstream>
#include <cmath>

namespace fs = std::filesystem;

namespace tinexus::monitor {

PowerMetrics PowerParser::parse_power(const std::string& base_sysfs) {
    PowerMetrics p;
    p.has_battery = false;
    p.battery_percent = 100;
    p.power_status = "AC Powered";
    p.power_watts = 0.0f;

    if (!fs::exists(base_sysfs)) {
        return p;
    }

    int battery_count = 0;
    int total_capacity = 0;
    std::string primary_status;
    double total_power_watts = 0.0;

    for (const auto& entry : fs::directory_iterator(base_sysfs)) {
        if (!entry.is_directory()) continue;

        // Check if power supply is a Battery
        fs::path type_path = entry.path() / "type";
        if (fs::exists(type_path)) {
            std::ifstream tf(type_path);
            std::string type;
            if (tf >> type && type == "Battery") {
                p.has_battery = true;
                battery_count++;

                // Read capacity
                fs::path cap_path = entry.path() / "capacity";
                if (fs::exists(cap_path)) {
                    std::ifstream cf(cap_path);
                    int cap = 0;
                    if (cf >> cap) {
                        total_capacity += cap;
                    }
                }

                // Read status (Charging, Discharging, Full, Not charging)
                fs::path status_path = entry.path() / "status";
                if (fs::exists(status_path)) {
                    std::ifstream sf(status_path);
                    std::string st;
                    if (sf >> st && primary_status.empty()) {
                        primary_status = st;
                    }
                }

                // Read power_now (Linux ABI: integer in micro-watts µW)
                fs::path power_path = entry.path() / "power_now";
                double watts = 0.0;
                if (fs::exists(power_path)) {
                    std::ifstream pf(power_path);
                    int64_t u_watts = 0;
                    if (pf >> u_watts && u_watts > 0) {
                        watts = static_cast<double>(u_watts) / 1000000.0;
                    }
                }

                // If power_now was unavailable, fallback to voltage_now (µV) * current_now (µA)
                if (watts <= 0.0) {
                    fs::path volt_path = entry.path() / "voltage_now";
                    fs::path curr_path = entry.path() / "current_now";
                    if (fs::exists(volt_path) && fs::exists(curr_path)) {
                        std::ifstream vf(volt_path);
                        std::ifstream crf(curr_path);
                        int64_t u_volts = 0;
                        int64_t u_amps = 0;
                        if ((vf >> u_volts) && (crf >> u_amps) && u_volts > 0 && u_amps > 0) {
                            watts = (static_cast<double>(u_volts) / 1000000.0) *
                                    (static_cast<double>(u_amps) / 1000000.0);
                        }
                    }
                }

                // Physical plausibility sanity check for laptop battery draw (0.1W - 250W)
                if (watts >= 0.1 && watts <= 250.0) {
                    total_power_watts += watts;
                }
            }
        }
    }

    if (battery_count > 0) {
        p.battery_percent = total_capacity / battery_count;
        p.power_status = primary_status.empty() ? "Discharging" : primary_status;
        p.power_watts = static_cast<float>(std::round(total_power_watts * 10.0) / 10.0);
    }

    return p;
}

} // namespace tinexus::monitor
