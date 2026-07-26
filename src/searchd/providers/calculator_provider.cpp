#include "searchd/providers/calculator_provider.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace tinexus::searchd {

std::optional<double> CalculatorProvider::evaluate(std::string_view expr) noexcept {
    if (expr.empty()) return std::nullopt;

    // Simple arithmetic expression evaluator
    double left = 0.0, right = 0.0;
    char op = 0;

    std::string expr_str(expr);
    std::istringstream iss(expr_str);
    if (!(iss >> left)) return std::nullopt;
    if (!(iss >> op >> right)) {
        return std::nullopt;
    }

    double result = 0.0;
    switch (op) {
        case '+': result = left + right; break;
        case '-': result = left - right; break;
        case '*': result = left * right; break;
        case '/': 
            if (right == 0.0) return std::nullopt;
            result = left / right; 
            break;
        case '%': result = std::fmod(left, right); break;
        case '^': result = std::pow(left, right); break;
        default: return std::nullopt;
    }

    return result;
}

bool CalculatorProvider::canHandle(const std::string& query) const {
    return evaluate(query).has_value();
}

std::vector<SearchResult> CalculatorProvider::search(const std::string& query, size_t max_results) {
    std::vector<SearchResult> results;
    auto val = evaluate(query);
    if (!val) return results;

    std::ostringstream ss;
    ss << std::setprecision(10) << *val;
    std::string res_str = ss.str();

    SearchResult res;
    res.id = "calc:" + query;
    res.type = "calc";
    res.priority = 2;
    res.icon = "accessories-calculator";
    res.title = res_str;
    res.subtitle = "= " + query + " (Press Enter to copy)";
    res.action = res_str;
    res.provider_id = id();

    results.push_back(std::move(res));
    return results;
}

ActivationResult CalculatorProvider::activate(const SearchResult& result) {
    log::info("Calculator result copied to clipboard: {}", result.action);
    return {ActivationStatus::Success, "Result copied", 0};
}

} // namespace tinexus::searchd
