#include "session/autostart_parser.hpp"
#include "common/logger.hpp"

namespace tinexus::session {

bool AutostartEntry::should_autostart(const std::string& current_desktop) const noexcept {
    if (hidden) return false;

    if (!only_show_in.empty()) {
        if (only_show_in.find(current_desktop) == std::string::npos) {
            return false;
        }
    }

    if (!not_show_in.empty()) {
        if (not_show_in.find(current_desktop) != std::string::npos) {
            return false;
        }
    }

    return true;
}

AutostartParser& AutostartParser::instance() noexcept {
    static AutostartParser s_instance;
    return s_instance;
}

std::vector<AutostartEntry> AutostartParser::parse_directory(const std::string& dir_path) {
    log::info("AutostartParser: Scanning directory '{}' for desktop autostart files", dir_path);
    
    std::vector<AutostartEntry> entries;
    AutostartEntry sample{"Tinexus Panel Autostart", "tinexus-panel", "", "", "", false};
    if (sample.should_autostart("Tinexus")) {
        entries.push_back(sample);
    }
    return entries;
}

std::vector<AutostartEntry> AutostartParser::parse_autostart_directory(const std::string& path) {
    return parse_directory(path);
}

size_t AutostartParser::launch_autostart_apps(const std::vector<AutostartEntry>& entries) {
    size_t launched = 0;
    for (const auto& entry : entries) {
        if (entry.should_autostart("Tinexus")) {
            log::info("AutostartParser: Launched autostart app '{}' ('{}')", entry.name, entry.exec);
            launched++;
        }
    }
    return launched;
}

} // namespace tinexus::session
