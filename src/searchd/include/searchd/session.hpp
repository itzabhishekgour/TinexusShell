#ifndef TINEXUS_SEARCH_SESSION_HPP
#define TINEXUS_SEARCH_SESSION_HPP

#include <string>
#include <map>
#include <chrono>

namespace tinexus::searchd {

struct StageTiming {
    std::string stage_name;
    uint64_t duration_us{0};
};

class SearchSession {
public:
    explicit SearchSession(std::string raw_query);

    void mark_stage(const std::string& stage_name);
    void set_cache_hit(bool hit) noexcept { m_cache_hit = hit; }
    void set_provider_count(uint32_t count) noexcept { m_provider_count = count; }
    void set_arena_usage(size_t bytes) noexcept { m_arena_usage_bytes = bytes; }

    void complete();
    std::string summary_json() const;

    [[nodiscard]] uint64_t total_latency_us() const noexcept { return m_total_latency_us; }
    [[nodiscard]] bool is_cache_hit() const noexcept { return m_cache_hit; }

private:
    std::string m_uuid;
    std::string m_raw_query;
    uint64_t m_query_hash{0};
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_stage_time;
    std::vector<StageTiming> m_timings;
    bool m_cache_hit{false};
    uint32_t m_provider_count{0};
    size_t m_arena_usage_bytes{0};
    uint64_t m_total_latency_us{0};
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_SESSION_HPP
