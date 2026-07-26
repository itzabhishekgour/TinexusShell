#include "searchd/cache.hpp"
#include <mutex>

namespace tinexus::searchd {

SearchCache::SearchCache(size_t capacity)
    : m_capacity(capacity) {}

std::optional<std::vector<SearchResult>> SearchCache::get(const std::string& fingerprint, uint64_t current_generation) {
    std::unique_lock lock(m_mutex);
    auto it = m_cache.find(fingerprint);
    if (it == m_cache.end()) {
        return std::nullopt;
    }

    // Check generation validity
    if (it->second.first.generation != current_generation) {
        m_lru_list.erase(it->second.second);
        m_cache.erase(it);
        return std::nullopt;
    }

    // Move to front (MRU)
    m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second.second);
    return it->second.first.results;
}

void SearchCache::put(std::string fingerprint, uint64_t current_generation, std::vector<SearchResult> results) {
    std::unique_lock lock(m_mutex);
    auto it = m_cache.find(fingerprint);

    if (it != m_cache.end()) {
        m_lru_list.splice(m_lru_list.begin(), m_lru_list, it->second.second);
        it->second.first = CacheEntry{current_generation, std::move(results)};
        return;
    }

    if (m_cache.size() >= m_capacity) {
        auto last = m_lru_list.back();
        m_cache.erase(last);
        m_lru_list.pop_back();
    }

    m_lru_list.push_front(fingerprint);
    m_cache[fingerprint] = {CacheEntry{current_generation, std::move(results)}, m_lru_list.begin()};
}

void SearchCache::clear() {
    std::unique_lock lock(m_mutex);
    m_cache.clear();
    m_lru_list.clear();
}

} // namespace tinexus::searchd
