#include "indexer/desktop_entry.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace tinexus::indexer {

static std::string trim(std::string_view sv) {
    auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = sv.find_last_not_of(" \t\r\n");
    return std::string(sv.substr(start, end - start + 1));
}

static std::vector<std::string> split_semicolon(std::string_view sv) {
    std::vector<std::string> results;
    std::string current;
    for (char ch : sv) {
        if (ch == ';') {
            std::string t = trim(current);
            if (!t.empty()) {
                results.push_back(std::move(t));
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    std::string t = trim(current);
    if (!t.empty()) {
        results.push_back(std::move(t));
    }
    return results;
}

std::string DesktopParser::sanitize_exec(std::string_view raw_exec) noexcept {
    std::string result;
    result.reserve(raw_exec.size());
    
    for (size_t i = 0; i < raw_exec.size(); ++i) {
        if (raw_exec[i] == '%' && i + 1 < raw_exec.size()) {
            char code = raw_exec[i + 1];
            if (code == 'f' || code == 'F' || code == 'u' || code == 'U' || 
                code == 'i' || code == 'c' || code == 'k' || code == 'm') {
                i++; // Skip field code
                continue;
            }
        }
        result.push_back(raw_exec[i]);
    }
    return trim(result);
}

std::optional<DesktopEntry> DesktopParser::parse_file(const std::filesystem::path& path) noexcept {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    DesktopEntry entry;
    entry.file_path = path;
    entry.desktop_id = path.stem().string();

    std::string line;
    bool in_desktop_entry_group = false;

    while (std::getline(file, line)) {
        std::string_view sv = line;
        sv = trim(sv);

        if (sv.empty() || sv.starts_with('#')) {
            continue; // Skip comments and empty lines
        }

        if (sv.starts_with('[')) {
            if (sv == "[Desktop Entry]") {
                in_desktop_entry_group = true;
            } else {
                in_desktop_entry_group = false; // Other groups like [Desktop Action]
            }
            continue;
        }

        if (!in_desktop_entry_group) {
            continue;
        }

        auto eq_pos = sv.find('=');
        if (eq_pos == std::string_view::npos) {
            continue;
        }

        std::string_view key = trim(sv.substr(0, eq_pos));
        std::string_view val = trim(sv.substr(eq_pos + 1));

        if (key == "Name") {
            if (entry.name.empty()) entry.name = std::string(val);
        } else if (key == "GenericName") {
            if (entry.generic_name.empty()) entry.generic_name = std::string(val);
        } else if (key == "Comment") {
            if (entry.comment.empty()) entry.comment = std::string(val);
        } else if (key == "Exec") {
            entry.exec = sanitize_exec(val);
        } else if (key == "Icon") {
            entry.icon = std::string(val);
        } else if (key == "NoDisplay") {
            entry.no_display = (val == "true" || val == "1");
        } else if (key == "Terminal") {
            entry.terminal = (val == "true" || val == "1");
        } else if (key == "Categories") {
            entry.categories = split_semicolon(val);
        } else if (key == "Keywords") {
            entry.keywords = split_semicolon(val);
        } else if (key == "X-Tinexus-Aliases" || key == "Aliases") {
            entry.aliases = split_semicolon(val);
        } else if (key == "Type") {
            if (val != "Application") {
                // Ignore non-application desktop entries (e.g. Directory, Link)
                return std::nullopt;
            }
        }
    }

    if (entry.name.empty() || entry.exec.empty() || entry.no_display) {
        return std::nullopt;
    }

    return entry;
}

} // namespace tinexus::indexer
