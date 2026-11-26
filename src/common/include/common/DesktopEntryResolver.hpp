#pragma once

// ============================================================================
// DesktopEntryResolver.hpp — Pure C++20 XDG .desktop Entry Indexer & Lookup
// ============================================================================
// Scans standard XDG application directories once (or on reload) to map
// Wayland app_ids, StartupWMClass values, and Exec binaries to display names
// and Freedesktop Icon= names. Zero Qt dependencies (safe for any daemon).
// ============================================================================

#include <common/AppId.hpp>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tinexus::common {

struct DesktopEntryInfo {
    std::string desktop_id;   // e.g. "firefox" or "tinexus-terminal"
    std::string name;         // e.g. "Firefox"
    std::string icon;         // e.g. "firefox" or "/usr/share/..."
    std::string exec;         // cleaned Exec without field codes (%u, %F)
    std::string wm_class;     // StartupWMClass if present
    bool        no_display{false};
};

class DesktopEntryResolver {
public:
    static DesktopEntryResolver& instance() {
        static DesktopEntryResolver inst;
        return inst;
    }

    void reload() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries.clear();
        m_by_key.clear();
        m_loaded = true;

        std::vector<std::filesystem::path> dirs;
        if (const char* xdg_home = std::getenv("XDG_DATA_HOME"); xdg_home && *xdg_home) {
            dirs.emplace_back(std::filesystem::path(xdg_home) / "applications");
        } else if (const char* home = std::getenv("HOME"); home && *home) {
            dirs.emplace_back(std::filesystem::path(home) / ".local/share/applications");
        }
        dirs.emplace_back("/usr/local/share/applications");
        dirs.emplace_back("/usr/share/applications");

        std::error_code ec;
        for (const auto& dir : dirs) {
            if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
                continue;
            }
            for (const auto& file : std::filesystem::recursive_directory_iterator(dir, ec)) {
                if (ec || !file.is_regular_file(ec)) continue;
                if (file.path().extension() != ".desktop") continue;
                if (auto parsed = parse_file(file.path())) {
                    index_entry(std::move(*parsed));
                }
            }
        }
    }

    std::optional<DesktopEntryInfo> resolve(std::string_view query) {
        if (query.empty()) return std::nullopt;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_loaded) {
            m_mutex.unlock();
            reload();
            m_mutex.lock();
        }

        const std::string q = to_lower(trim(query));
        if (auto idx = lookup_locked(q)) return m_entries[*idx];

        const std::string stripped = strip_desktop_suffix(q);
        if (auto idx = lookup_locked(stripped)) return m_entries[*idx];

        const std::string base_exec = extract_binary_name(stripped);
        if (auto idx = lookup_locked(base_exec)) return m_entries[*idx];

        // Reverse-DNS tail (e.g. "org.mozilla.firefox" -> "firefox")
        if (auto dot = stripped.rfind('.'); dot != std::string::npos && dot + 1 < stripped.size()) {
            const std::string tail = stripped.substr(dot + 1);
            if (auto idx = lookup_locked(tail)) return m_entries[*idx];
        }

        const std::string canonical = get_canonical_app_id(stripped);
        if (auto idx = lookup_locked(canonical)) return m_entries[*idx];

        return std::nullopt;
    }

private:
    DesktopEntryResolver() = default;

    static std::string to_lower(std::string_view s) {
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
        return out;
    }

    static std::string trim(std::string_view s) {
        size_t a = 0;
        while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
        size_t b = s.size();
        while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
        return std::string(s.substr(a, b - a));
    }

    static std::string strip_desktop_suffix(std::string_view s) {
        constexpr std::string_view kExt = ".desktop";
        if (s.size() > kExt.size() && s.compare(s.size() - kExt.size(), kExt.size(), kExt) == 0) {
            return std::string(s.substr(0, s.size() - kExt.size()));
        }
        return std::string(s);
    }

    static std::string clean_exec(std::string_view raw) {
        std::string out;
        out.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '%' && i + 1 < raw.size()) {
                ++i;
                continue;
            }
            out.push_back(raw[i]);
        }
        return trim(out);
    }

    static std::string extract_binary_name(std::string_view exec_line) {
        std::string tok;
        std::vector<std::string> tokens;
        for (char ch : exec_line) {
            if (std::isspace(static_cast<unsigned char>(ch))) {
                if (!tok.empty()) {
                    tokens.push_back(tok);
                    tok.clear();
                }
            } else {
                tok.push_back(ch);
            }
        }
        if (!tok.empty()) tokens.push_back(tok);

        for (size_t i = 0; i < tokens.size(); ++i) {
            const std::string& t = tokens[i];
            if (t == "env" || t == "sudo" || t == "-E") continue;
            if (t.find('=') != std::string::npos) continue;
            if (!t.empty() && t[0] == '-') continue;
            auto slash = t.rfind('/');
            return to_lower(slash == std::string::npos ? t : t.substr(slash + 1));
        }
        return {};
    }

    static std::optional<DesktopEntryInfo> parse_file(const std::filesystem::path& path) {
        std::ifstream f(path);
        if (!f.is_open()) return std::nullopt;

        DesktopEntryInfo info;
        info.desktop_id = path.stem().string();
        bool in_desktop_entry = false;
        bool hidden = false;
        std::string type;
        std::string line;

        while (std::getline(f, line)) {
            std::string t = trim(line);
            if (t.empty() || t[0] == '#') continue;
            if (t.front() == '[') {
                in_desktop_entry = (t == "[Desktop Entry]");
                continue;
            }
            if (!in_desktop_entry) continue;

            auto eq = t.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = trim(t.substr(0, eq));
            const std::string val = trim(t.substr(eq + 1));

            if (key == "Type") type = val;
            else if (key == "Hidden") hidden = (val == "true" || val == "1");
            else if (key == "NoDisplay") info.no_display = (val == "true" || val == "1");
            else if (key == "Name" && info.name.empty()) info.name = val;
            else if (key == "Icon" && info.icon.empty()) info.icon = val;
            else if (key == "Exec" && info.exec.empty()) info.exec = clean_exec(val);
            else if (key == "StartupWMClass" && info.wm_class.empty()) info.wm_class = val;
        }

        if (hidden || type != "Application" || info.name.empty()) {
            return std::nullopt;
        }
        return info;
    }

    static int entry_priority(const DesktopEntryInfo& e) {
        int score = 0;
        if (!e.no_display) score += 100;
        if (!e.icon.empty()) score += 20;
        if (e.desktop_id.starts_with("tinexus-")) score += 10;
        return score;
    }

    void index_key(const std::string& raw_key, size_t idx) {
        if (raw_key.empty()) return;
        const std::string key = to_lower(raw_key);
        auto it = m_by_key.find(key);
        if (it == m_by_key.end() || entry_priority(m_entries[idx]) > entry_priority(m_entries[it->second])) {
            m_by_key[key] = idx;
        }
    }

    void index_entry(DesktopEntryInfo info) {
        const size_t idx = m_entries.size();
        const std::string id_low = to_lower(info.desktop_id);
        const std::string wm_low = to_lower(info.wm_class);
        const std::string bin_low = extract_binary_name(info.exec);
        const std::string can_low = get_canonical_app_id(id_low);

        m_entries.push_back(std::move(info));

        index_key(id_low, idx);
        index_key(wm_low, idx);
        index_key(bin_low, idx);
        index_key(can_low, idx);
        if (auto dot = id_low.rfind('.'); dot != std::string::npos && dot + 1 < id_low.size()) {
            index_key(id_low.substr(dot + 1), idx);
        }
    }

    std::optional<size_t> lookup_locked(const std::string& key) const {
        if (key.empty()) return std::nullopt;
        auto it = m_by_key.find(key);
        if (it != m_by_key.end()) return it->second;
        return std::nullopt;
    }

    std::mutex m_mutex;
    bool m_loaded{false};
    std::vector<DesktopEntryInfo> m_entries;
    std::unordered_map<std::string, size_t> m_by_key;
};

} // namespace tinexus::common
