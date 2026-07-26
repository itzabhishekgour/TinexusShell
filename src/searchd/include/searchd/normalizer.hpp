#ifndef TINEXUS_SEARCH_NORMALIZER_HPP
#define TINEXUS_SEARCH_NORMALIZER_HPP

#include <string>
#include <string_view>
#include <optional>
#include <unordered_map>

namespace tinexus::searchd {

struct NormalizedQuery {
    std::string raw_query;
    std::string clean_query;
    std::string prefix_tag;      // e.g. "app", "calc", "sys", "file", "clip"
    std::string expanded_query;  // Query with alias expansion
    bool has_prefix{false};
};

class QueryNormalizer {
public:
    static QueryNormalizer& instance() noexcept;

    NormalizedQuery normalize(std::string_view raw_query) const;
    void register_alias(std::string alias, std::string target);

private:
    QueryNormalizer();
    std::unordered_map<std::string, std::string> m_aliases;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_NORMALIZER_HPP
