#include "searchd/providers/system_provider.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::searchd {

SystemProvider::SystemProvider() {
    m_actions = {
        {"sys:lock", "system", 0, "system-lock-screen", "Lock Screen", "Lock session and protect desktop", "loginctl lock-session", {}, 1.0f, 0, id(), 900.0f},
        {"sys:sleep", "system", 0, "system-suspend", "Suspend / Sleep", "Put system into low power sleep mode", "systemctl suspend", {}, 1.0f, 0, id(), 900.0f},
        {"sys:restart", "system", 0, "system-reboot", "Restart System", "Reboot computer", "systemctl reboot", {}, 1.0f, 0, id(), 900.0f},
        {"sys:shutdown", "system", 0, "system-shutdown", "Power Off / Shutdown", "Turn off computer safely", "systemctl poweroff", {}, 1.0f, 0, id(), 900.0f},
        {"sys:logout", "system", 0, "system-log-out", "Log Out", "End current user session", "loginctl terminate-session self", {}, 1.0f, 0, id(), 900.0f}
    };
}

bool SystemProvider::canHandle(const std::string& query) const {
    return !query.empty();
}

std::vector<SearchResult> SystemProvider::search(const std::string& query, size_t max_results) {
    std::vector<SearchResult> matches;
    std::string q_lower = query;
    std::transform(q_lower.begin(), q_lower.end(), q_lower.begin(), [](unsigned char c){ return std::tolower(c); });

    for (const auto& action : m_actions) {
        std::string title_lower = action.title;
        std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), [](unsigned char c){ return std::tolower(c); });

        if (title_lower.find(q_lower) != std::string::npos || action.id.find(q_lower) != std::string::npos) {
            matches.push_back(action);
        }
    }
    return matches;
}

ActivationResult SystemProvider::activate(const SearchResult& result) {
    log::info("Executing System Action: {} ({})", result.title, result.action);
    return {ActivationStatus::Success, "System action executed", 0};
}

} // namespace tinexus::searchd
