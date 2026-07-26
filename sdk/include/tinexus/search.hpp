#ifndef TINEXUS_SDK_SEARCH_HPP
#define TINEXUS_SDK_SEARCH_HPP

#include <string>
#include <vector>
#include <future>
#include "tinexus/result.hpp"

namespace tinexus {

class SearchResultItem {
public:
    SearchResultItem(std::string id, std::string title, std::string category, double score)
        : m_id(std::move(id)), m_title(std::move(title)), m_category(std::move(category)), m_score(score) {}

    [[nodiscard]] const std::string& id() const noexcept { return m_id; }
    [[nodiscard]] const std::string& title() const noexcept { return m_title; }
    [[nodiscard]] const std::string& category() const noexcept { return m_category; }
    [[nodiscard]] double score() const noexcept { return m_score; }

private:
    std::string m_id;
    std::string m_title;
    std::string m_category;
    double m_score{0.0};
};

class SearchService {
public:
    SearchService() = default;
    ~SearchService() = default;

    Result<std::vector<SearchResultItem>> query(const std::string& term);
    std::future<Result<std::vector<SearchResultItem>>> query_async(const std::string& term);
};

} // namespace tinexus

#endif // TINEXUS_SDK_SEARCH_HPP
