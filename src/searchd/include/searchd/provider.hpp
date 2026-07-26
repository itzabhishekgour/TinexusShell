#ifndef TINEXUS_SEARCH_PROVIDER_HPP
#define TINEXUS_SEARCH_PROVIDER_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

namespace tinexus::searchd {

enum class ActivationStatus {
    Success,
    PermissionDenied,
    NotFound,
    Failed,
    Retry,
    Cancelled
};

struct ActivationResult {
    ActivationStatus status{ActivationStatus::Success};
    std::string message;
    int pid{-1};
};

struct SearchResult {
    std::string id;           // e.g. "app:org.mozilla.firefox"
    std::string type;         // e.g. "app", "calc", "system", "file"
    int priority{0};          // Lower = higher base priority
    std::string icon;         // Icon theme name or path
    std::string title;        // Primary display text
    std::string subtitle;     // Description or execution details
    std::string action;       // Exec command or action payload
    std::map<std::string, std::string> metadata;
    float confidence{1.0f};   // Intent / match confidence score
    uint32_t latency_us{0};   // Provider execution time in microseconds
    std::string provider_id;  // Producing provider ID (e.g. "apps", "calculator")
    float score{0.0f};        // Computed final ranking score
};

struct ProviderHealthStats {
    uint64_t query_count{0};
    uint64_t total_latency_us{0};
    uint32_t timeout_count{0};
    uint32_t error_count{0};
    double avg_latency_ms{0.0};
};

class ISearchProvider {
public:
    virtual ~ISearchProvider() = default;

    virtual std::string id() const = 0;
    virtual std::string displayName() const = 0;
    virtual std::string iconName() const = 0;
    virtual int priority() const = 0;

    virtual bool initialize() { return true; }
    virtual void shutdown() {}
    virtual bool reload() { return true; }
    virtual ProviderHealthStats health() const { return m_stats; }

    virtual bool canHandle(const std::string& query) const = 0;
    virtual std::vector<SearchResult> search(const std::string& query, size_t max_results) = 0;
    virtual ActivationResult activate(const SearchResult& result) = 0;

protected:
    ProviderHealthStats m_stats;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_PROVIDER_HPP
