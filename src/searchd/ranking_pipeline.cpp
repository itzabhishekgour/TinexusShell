#include "searchd/ranking_stage.hpp"
#include <algorithm>
#include <cmath>

namespace tinexus::searchd {

RankingPipeline::RankingPipeline() : m_config(RankingConfig{}) {}

RankingPipeline::RankingPipeline(RankingConfig config) : m_config(config) {}

float RankingPipeline::calculate_trigram_score(std::string_view query, std::string_view title) noexcept {
    if (query.empty() || title.empty()) return 0.0f;
    if (query.size() < 3 || title.size() < 3) {
        return title.find(query) != std::string_view::npos ? 0.8f : 0.0f;
    }

    size_t q_trigrams = query.size() - 2;
    size_t t_trigrams = title.size() - 2;
    size_t matches = 0;

    for (size_t i = 0; i <= query.size() - 3; ++i) {
        std::string_view tri = query.substr(i, 3);
        if (title.find(tri) != std::string_view::npos) {
            matches++;
        }
    }

    return (2.0f * static_cast<float>(matches)) / static_cast<float>(q_trigrams + t_trigrams);
}

int RankingPipeline::calculate_levenshtein_distance(std::string_view s1, std::string_view s2) noexcept {
    size_t len1 = s1.size();
    size_t len2 = s2.size();

    std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1));

    for (size_t i = 0; i <= len1; ++i) d[i][0] = static_cast<int>(i);
    for (size_t j = 0; j <= len2; ++j) d[0][j] = static_cast<int>(j);

    for (size_t i = 1; i <= len1; ++i) {
        for (size_t j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = std::min({ d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost });
        }
    }

    return d[len1][len2];
}

void RankingPipeline::rank(const NormalizedQuery& query, std::vector<SearchResult>& candidates) const {
    const std::string& q = query.clean_query;

    for (auto& item : candidates) {
        float match_score = 0.0f;
        std::string title_lower = item.title;
        std::transform(title_lower.begin(), title_lower.end(), title_lower.begin(), [](unsigned char c){ return std::tolower(c); });

        // Stage 1: Text Match (Exact, Prefix, Substring, or Trigram)
        if (title_lower == q || title_lower == query.expanded_query) {
            match_score = 1.0f;
        } else if (title_lower.starts_with(q) || title_lower.starts_with(query.expanded_query)) {
            match_score = 0.9f;
        } else {
            match_score = calculate_trigram_score(q, title_lower);
        }

        // Base Category Weights
        float base_weight = 700.0f;
        if (item.type == "calc") base_weight = 850.0f;
        else if (item.type == "system") base_weight = 900.0f;
        else if (item.type == "app") base_weight = (match_score >= 0.95f) ? 1000.0f : 750.0f;

        // Stage 2: Prefix Match Bonus
        float prefix_bonus = title_lower.starts_with(q) ? m_config.prefix_bonus : 0.0f;

        // Stage 3: Frequency & Recency Factors
        float final_score = (match_score * base_weight) + prefix_bonus;

        if (item.metadata.count("pinned") && item.metadata["pinned"] == "true") {
            final_score += m_config.pinned_bonus;
        }

        item.score = final_score;
    }

    // Top-20 Partial Sort for sub-millisecond efficiency
    size_t top_k = std::min(candidates.size(), static_cast<size_t>(20));
    std::partial_sort(candidates.begin(), candidates.begin() + static_cast<ptrdiff_t>(top_k), candidates.end(), [](const SearchResult& a, const SearchResult& b) {
        return a.score > b.score;
    });
}

} // namespace tinexus::searchd
