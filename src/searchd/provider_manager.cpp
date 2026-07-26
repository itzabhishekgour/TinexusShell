#include "searchd/provider_manager.hpp"
#include "common/logger.hpp"
#include <thread>
#include <chrono>

namespace tinexus::searchd {

ProviderManager::ProviderManager() = default;
ProviderManager::~ProviderManager() = default;

void ProviderManager::register_provider(std::shared_ptr<ISearchProvider> provider) {
    if (provider && provider->initialize()) {
        m_providers.push_back(provider);
        log::info("Registered search provider: {} ({})", provider->displayName(), provider->id());
    }
}

void ProviderManager::unregister_provider(const std::string& provider_id) {
    std::erase_if(m_providers, [&provider_id](const std::shared_ptr<ISearchProvider>& p) {
        if (p->id() == provider_id) {
            p->shutdown();
            return true;
        }
        return false;
    });
}

std::vector<SearchResult> ProviderManager::execute_search(const NormalizedQuery& query, size_t max_results_per_provider, SearchSession& session) {
    std::vector<SearchResult> all_candidates;
    session.set_provider_count(static_cast<uint32_t>(m_providers.size()));

    for (const auto& provider : m_providers) {
        if (provider->canHandle(query.clean_query) || provider->canHandle(query.raw_query)) {
            try {
                auto results = provider->search(query.clean_query, max_results_per_provider);
                all_candidates.insert(all_candidates.end(), results.begin(), results.end());
            } catch (...) {
                log::warn("Exception caught during provider execution: {}", provider->id());
            }
        }
    }

    return all_candidates;
}

std::map<std::string, ProviderHealthStats> ProviderManager::get_all_health_stats() const {
    std::map<std::string, ProviderHealthStats> stats;
    for (const auto& p : m_providers) {
        stats[p->id()] = p->health();
    }
    return stats;
}

} // namespace tinexus::searchd
