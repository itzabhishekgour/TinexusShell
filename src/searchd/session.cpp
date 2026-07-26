#include "searchd/session.hpp"
#include "common/logger.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <functional>

namespace tinexus::searchd {

static std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    const char* hex_chars = "0123456789abcdef";

    std::string uuid;
    uuid.reserve(36);
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            uuid += '-';
        } else {
            uuid += hex_chars[dis(gen)];
        }
    }
    return uuid;
}

SearchSession::SearchSession(std::string raw_query)
    : m_uuid(generate_uuid()),
      m_raw_query(std::move(raw_query)),
      m_query_hash(std::hash<std::string>{}(m_raw_query)),
      m_start_time(std::chrono::steady_clock::now()),
      m_last_stage_time(m_start_time) {}

void SearchSession::mark_stage(const std::string& stage_name) {
    auto now = std::chrono::steady_clock::now();
    uint64_t duration = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - m_last_stage_time).count());
    m_timings.push_back({stage_name, duration});
    m_last_stage_time = now;
}

void SearchSession::complete() {
    auto now = std::chrono::steady_clock::now();
    m_total_latency_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - m_start_time).count());
}

std::string SearchSession::summary_json() const {
    std::ostringstream ss;
    ss << "{\"uuid\":\"" << m_uuid << "\","
       << "\"query\":\"" << m_raw_query << "\","
       << "\"total_latency_us\":" << m_total_latency_us << ","
       << "\"cache_hit\":" << (m_cache_hit ? "true" : "false") << ","
       << "\"provider_count\":" << m_provider_count << ","
       << "\"arena_bytes\":" << m_arena_usage_bytes << "}";
    return ss.str();
}

} // namespace tinexus::searchd
