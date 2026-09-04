#include "searchd/providers/app_provider.hpp"
#include "indexer/ram_snapshot.hpp"
#include "common/logger.hpp"
#include <algorithm>
#include <unistd.h>

namespace tinexus::searchd {

bool AppProvider::canHandle(const std::string& query) const {
    return true;
}

static bool contains_icase(std::string_view text, std::string_view query) {
    if (query.empty()) return true;
    auto it = std::search(
        text.begin(), text.end(),
        query.begin(), query.end(),
        [](unsigned char ch1, unsigned char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
    );
    return it != text.end();
}

std::vector<SearchResult> AppProvider::search(const std::string& query, size_t max_results) {
    std::vector<SearchResult> results;
    size_t candidate_cap = max_results * 5;

    tinexus::indexer::RamSnapshot::instance().for_matching_entries(query, [&results, &query, candidate_cap](const tinexus::indexer::DesktopEntry& entry) {
        if (results.size() >= candidate_cap) return;

        if (!contains_icase(entry.name, query) && 
            !contains_icase(entry.exec, query) && 
            !contains_icase(entry.desktop_id, query) &&
            !contains_icase(entry.generic_name, query)) {
            return;
        }

        SearchResult res;
        res.id = "app:" + entry.desktop_id;
        res.type = "app";
        res.priority = 1;
        res.icon = entry.icon.empty() ? "application-x-executable" : entry.icon;
        res.title = entry.name;
        res.subtitle = entry.generic_name.empty() ? entry.exec : entry.generic_name;
        res.action = entry.exec;
        res.provider_id = "apps";

        results.push_back(std::move(res));
    });

    struct BuiltinApp { const char* name; const char* exec; const char* desc; };
    BuiltinApp builtins[] = {
        {"Tinexus Terminal", "tinexus-terminal", "Default Wayland Terminal"},
        {"Tinexus Settings", "tinexus-settings-ui", "System Configuration"},
        {"Files", "tinexus-files", "File Manager"},
        {"Lock Screen", "tinexus-lock", "Lock the session"},
        {"Screenshot", "tinexus-screenshot", "Capture screen"}
    };
    for (const auto& b : builtins) {
        if (contains_icase(b.name, query) || contains_icase(b.exec, query) || contains_icase(b.desc, query)) {
            SearchResult res;
            res.id = std::string("builtin:") + b.exec;
            res.type = "app";
            res.priority = 2;
            res.icon = "application-x-executable";
            res.title = b.name;
            res.subtitle = b.desc;
            res.action = b.exec;
            res.provider_id = "apps";
            results.push_back(std::move(res));
        }
    }

    return results;
}

ActivationResult AppProvider::activate(const SearchResult& result) {
    log::info("Launching application: {} ({})", result.title, result.action);

    pid_t pid = fork();
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", result.action.c_str(), nullptr);
        _exit(127);
    } else if (pid > 0) {
        return {ActivationStatus::Success, "Application launched", pid};
    }

    return {ActivationStatus::Failed, "Failed to fork process", -1};
}

} // namespace tinexus::searchd
