#ifndef TINEXUS_SEARCH_APP_PROVIDER_HPP
#define TINEXUS_SEARCH_APP_PROVIDER_HPP

#include "searchd/provider.hpp"

namespace tinexus::searchd {

class AppProvider : public ISearchProvider {
public:
    AppProvider() = default;

    std::string id() const override { return "apps"; }
    std::string displayName() const override { return "Applications"; }
    std::string iconName() const override { return "system-run"; }
    int priority() const override { return 1; }

    bool canHandle(const std::string& query) const override;
    std::vector<SearchResult> search(const std::string& query, size_t max_results) override;
    ActivationResult activate(const SearchResult& result) override;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_APP_PROVIDER_HPP
