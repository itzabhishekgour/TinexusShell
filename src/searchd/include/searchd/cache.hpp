#ifndef TINEXUS_SEARCH_CACHE_HPP
#define TINEXUS_SEARCH_CACHE_HPP

#include "searchd/provider.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <shared_mutex>
#include <list>
#include <optional>

namespace tinexus::searchd {

struct CacheEntry {
    uint64_t generation{0};
    std::vector<SearchResult> results;
};

class SearchCache {
public:
    explicit SearchCache(size_t capacity = 512);

    std::optional<std::vector<SearchResult>> get(const std::string& fingerprint, uint64_t current_generation);
    void put(std::string fingerprint, uint64_t current_generation, std::vector<SearchResult> results);
    void clear();

private:
    size_t m_capacity;
    mutable std::shared_mutex m_mutex;
    std::list<std::string> m_lru_list;
    std::unordered_map<std::string, std::pair<CacheEntry, std::list<std::string>::iterator>> m_cache;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_CACHE_HPP
