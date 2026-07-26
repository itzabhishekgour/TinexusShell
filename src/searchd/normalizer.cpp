#include "searchd/normalizer.hpp"
#include <algorithm>
#include <cctype>

namespace tinexus::searchd {

static std::string to_lower_trim(std::string_view sv) {
    auto start = sv.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = sv.find_last_not_of(" \t\r\n");
    std::string res(sv.substr(start, end - start + 1));
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c){ return std::tolower(c); });
    return res;
}

QueryNormalizer::QueryNormalizer() {
    // Default built-in aliases
    m_aliases["chrome"] = "google chrome";
    m_aliases["vscode"] = "visual studio code";
    m_aliases["code"] = "visual studio code";
    m_aliases["ff"] = "firefox";
    m_aliases["browser"] = "google chrome";
    m_aliases["calc"] = "calculator";
}

QueryNormalizer& QueryNormalizer::instance() noexcept {
    static QueryNormalizer s_instance;
    return s_instance;
}

void QueryNormalizer::register_alias(std::string alias, std::string target) {
    std::transform(alias.begin(), alias.end(), alias.begin(), [](unsigned char c){ return std::tolower(c); });
    m_aliases[alias] = std::move(target);
}

NormalizedQuery QueryNormalizer::normalize(std::string_view raw_query) const {
    NormalizedQuery norm;
    norm.raw_query = std::string(raw_query);
    std::string clean = to_lower_trim(raw_query);

    // Check for query prefix tag (e.g. app:chrome, calc:2+2, sys:lock)
    auto colon_pos = clean.find(':');
    if (colon_pos != std::string::npos && colon_pos > 0) {
        std::string tag = clean.substr(0, colon_pos);
        if (tag == "app" || tag == "calc" || tag == "sys" || tag == "file" || tag == "clip" || tag == "ai") {
            norm.has_prefix = true;
            norm.prefix_tag = tag;
            clean = to_lower_trim(clean.substr(colon_pos + 1));
        }
    }

    norm.clean_query = clean;
    norm.expanded_query = clean;

    auto it = m_aliases.find(clean);
    if (it != m_aliases.end()) {
        norm.expanded_query = it->second;
    }

    return norm;
}

} // namespace tinexus::searchd
