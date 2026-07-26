#include "session/autostart_parser.hpp"
#include "common/logger.hpp"

namespace tinexus::session {

AutostartParser& AutostartParser::instance() noexcept {
    static AutostartParser s_instance;
    return s_instance;
}

std::vector<AutostartEntry> AutostartParser::parse_autostart_directory(const std::string& path) {
    log::info("AutostartParser: Scanning autostart directory '{}'...", path);
    std::vector<AutostartEntry> entries;
    // Example placeholder autostart entry
    entries.push_back(AutostartEntry{"Tinexus Wallpaper Daemon", "tinexus-wallpaper", false, true});
    return entries;
}

size_t AutostartParser::launch_autostart_apps(const std::vector<AutostartEntry>& entries) {
    size_t launched = 0;
    for (const auto& entry : entries) {
        if (!entry.hidden && entry.only_show_in_tinexus) {
            log::info("AutostartParser: Launching autostart app '{}' -> Exec: {}", entry.name, entry.exec);
            launched++;
        }
    }
    return launched;
}

} // namespace tinexus::session
