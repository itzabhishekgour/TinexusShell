#ifndef TINEXUS_SEARCH_PROVIDER_MANAGER_HPP
#define TINEXUS_SEARCH_PROVIDER_MANAGER_HPP

#include "searchd/provider.hpp"
#include "searchd/normalizer.hpp"
#include "searchd/session.hpp"
#include <vector>
#include <memory>
#include <future>

namespace tinexus::searchd {

class ProviderManager {
public:
    ProviderManager();
    ~ProviderManager();

    void register_provider(std::shared_ptr<ISearchProvider> provider);
    void unregister_provider(const std::string& provider_id);

    std::vector<SearchResult> execute_search(const NormalizedQuery& query, size_t max_results_per_provider, SearchSession& session);

    [[nodiscard]] std::map<std::string, ProviderHealthStats> get_all_health_stats() const;

private:
    std::vector<std::shared_ptr<ISearchProvider>> m_providers;
    uint32_t m_timeout_ms{10}; // Hard 10ms timeout per provider
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_PROVIDER_MANAGER_HPP
