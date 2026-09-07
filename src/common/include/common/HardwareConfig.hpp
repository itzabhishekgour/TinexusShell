#pragma once

#include <string>
#include <cstdint>

namespace tinexus::hardware {

struct HardwareSettings {
    int  volume{75};      // 0 - 100%
    bool muted{false};
    int  brightness{80};  // 5 - 100%
};

class HardwareConfig {
public:
    static std::string default_config_path();
    static HardwareSettings load(const std::string& custom_path = "");
    static bool save(const HardwareSettings& settings, const std::string& custom_path = "");
};

} // namespace tinexus::hardware
