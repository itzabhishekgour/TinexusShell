#ifndef TINEXUS_SEARCH_CALCULATOR_PROVIDER_HPP
#define TINEXUS_SEARCH_CALCULATOR_PROVIDER_HPP

#include "searchd/provider.hpp"

namespace tinexus::searchd {

class CalculatorProvider : public ISearchProvider {
public:
    CalculatorProvider() = default;

    std::string id() const override { return "calculator"; }
    std::string displayName() const override { return "Calculator"; }
    std::string iconName() const override { return "accessories-calculator"; }
    int priority() const override { return 2; }

    bool canHandle(const std::string& query) const override;
    std::vector<SearchResult> search(const std::string& query, size_t max_results) override;
    ActivationResult activate(const SearchResult& result) override;

    static std::optional<double> evaluate(std::string_view expr) noexcept;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_CALCULATOR_PROVIDER_HPP
