#ifndef TINEXUS_SEARCH_RANKING_STAGE_HPP
#define TINEXUS_SEARCH_RANKING_STAGE_HPP

#include "searchd/provider.hpp"
#include "searchd/normalizer.hpp"
#include <vector>
#include <string>

namespace tinexus::searchd {

struct RankingConfig {
    float prefix_bonus{200.0f};
    float exact_match_bonus{300.0f};
    float pinned_bonus{300.0f};
    float recency_max_bonus{400.0f};
    float recency_lambda{0.00412f}; // 7-day half life
    float frequency_weight{0.1f};
};

class RankingPipeline {
public:
    RankingPipeline();
    explicit RankingPipeline(RankingConfig config);

    void rank(const NormalizedQuery& query, std::vector<SearchResult>& candidates) const;

    // Individual scoring stages
    static float calculate_trigram_score(std::string_view query, std::string_view title) noexcept;
    static int calculate_levenshtein_distance(std::string_view s1, std::string_view s2) noexcept;

private:
    RankingConfig m_config;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_RANKING_STAGE_HPP
