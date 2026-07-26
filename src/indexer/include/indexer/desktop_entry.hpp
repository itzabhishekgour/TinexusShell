#ifndef TINEXUS_INDEXER_DESKTOP_ENTRY_HPP
#define TINEXUS_INDEXER_DESKTOP_ENTRY_HPP

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace tinexus::indexer {

struct DesktopEntry {
    std::string desktop_id;      // e.g. "org.mozilla.firefox"
    std::string name;            // Display Name e.g. "Firefox Web Browser"
    std::string generic_name;    // e.g. "Web Browser"
    std::string comment;         // e.g. "Browse the World Wide Web"
    std::string exec;            // Command line execution string
    std::string icon;            // Icon name or absolute path
    std::vector<std::string> categories; // e.g. ["Network", "WebBrowser"]
    std::vector<std::string> keywords;   // e.g. ["internet", "surf", "web"]
    std::vector<std::string> aliases;    // e.g. ["ff", "browser"]
    bool no_display{false};      // Hidden from app grid/launcher if true
    bool terminal{false};        // Runs inside terminal if true
    std::filesystem::path file_path;
    uint64_t last_modified_time{0};
};

class DesktopParser {
public:
    // Parses a single .desktop file robustly. Returns std::nullopt if file is invalid or unparseable.
    // Never throws exceptions or crashes on corrupt/malformed files.
    static std::optional<DesktopEntry> parse_file(const std::filesystem::path& path) noexcept;
    
    // Normalizes Exec fields (removes %u, %f, %F, %U field codes)
    static std::string sanitize_exec(std::string_view raw_exec) noexcept;
};

} // namespace tinexus::indexer

#endif // TINEXUS_INDEXER_DESKTOP_ENTRY_HPP
