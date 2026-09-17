#pragma once

#include <string>
#include <string_view>
#include <algorithm>
#include <cctype>

namespace tinexus::common {

inline std::string get_canonical_app_id(std::string_view name_or_cmd) {
    std::string lower;
    lower.reserve(name_or_cmd.size());
    for (char c : name_or_cmd) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    if (lower.find("settings") != std::string::npos) return "tinexus-settings";
    if (lower.find("about") != std::string::npos) return "tinexus-about";
    if (lower.find("monitor") != std::string::npos) return "tinexus-monitor";
    if (lower.find("launcher") != std::string::npos) return "tinexus-launcher";
    if (lower.find("files") != std::string::npos) return "tinexus-files";
    if (lower.find("terminal") != std::string::npos) return "tinexus-terminal";
    if (lower.find("store") != std::string::npos || lower.find("pkg") != std::string::npos) return "tinexus-store";
    if (lower.find("lock") != std::string::npos) return "tinexus-lock";
    return std::string(name_or_cmd);
}

inline bool is_single_instance_app(std::string_view app_id) {
    return app_id == "tinexus-settings" ||
           app_id == "tinexus-about" ||
           app_id == "tinexus-monitor" ||
           app_id == "tinexus-launcher" ||
           app_id == "tinexus-store" ||
           app_id == "tinexus-lock";
}

} // namespace tinexus::common
