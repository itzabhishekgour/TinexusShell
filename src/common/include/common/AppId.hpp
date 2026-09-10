#pragma once

#include <string>
#include <string_view>

namespace tinexus::common {

inline std::string get_canonical_app_id(std::string_view name_or_cmd) {
    if (name_or_cmd.find("settings") != std::string_view::npos) return "tinexus-settings";
    if (name_or_cmd.find("about") != std::string_view::npos) return "tinexus-about";
    if (name_or_cmd.find("monitor") != std::string_view::npos) return "tinexus-monitor";
    if (name_or_cmd.find("launcher") != std::string_view::npos) return "tinexus-launcher";
    if (name_or_cmd.find("files") != std::string_view::npos) return "tinexus-files";
    if (name_or_cmd.find("terminal") != std::string_view::npos) return "tinexus-terminal";
    if (name_or_cmd.find("store") != std::string_view::npos || name_or_cmd.find("pkg") != std::string_view::npos) return "tinexus-store";
    if (name_or_cmd.find("lock") != std::string_view::npos) return "tinexus-lock";
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
