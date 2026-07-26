#ifndef TINEXUS_SESSION_AUTOSTART_PARSER_HPP
#define TINEXUS_SESSION_AUTOSTART_PARSER_HPP

#include <string>
#include <vector>

namespace tinexus::session {

struct AutostartEntry {
    std::string name;
    std::string exec;
    bool hidden{false};
    bool only_show_in_tinexus{true};
};

class AutostartParser {
public:
    static AutostartParser& instance() noexcept;

    AutostartParser() = default;
    ~AutostartParser() = default;

    std::vector<AutostartEntry> parse_autostart_directory(const std::string& path);
    size_t launch_autostart_apps(const std::vector<AutostartEntry>& entries);
};

} // namespace tinexus::session

#endif // TINEXUS_SESSION_AUTOSTART_PARSER_HPP
