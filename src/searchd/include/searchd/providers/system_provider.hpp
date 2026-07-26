#ifndef TINEXUS_SEARCH_SYSTEM_PROVIDER_HPP
#define TINEXUS_SEARCH_SYSTEM_PROVIDER_HPP

#include "searchd/provider.hpp"

namespace tinexus::searchd {

class SystemProvider : public ISearchProvider {
public:
    SystemProvider();

    std::string id() const override { return "system"; }
    std::string displayName() const override { return "System Actions"; }
    std::string iconName() const override { return "system-shutdown"; }
    int priority() const override { return 0; }

    bool canHandle(const std::string& query) const override;
    std::vector<SearchResult> search(const std::string& query, size_t max_results) override;
    ActivationResult activate(const SearchResult& result) override;

private:
    std::vector<SearchResult> m_actions;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_SYSTEM_PROVIDER_HPP
