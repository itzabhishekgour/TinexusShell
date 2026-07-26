#ifndef TINEXUS_SEARCH_INTENT_DETECTOR_HPP
#define TINEXUS_SEARCH_INTENT_DETECTOR_HPP

#include "searchd/normalizer.hpp"
#include <string>
#include <unordered_map>

namespace tinexus::searchd {

enum class PrimaryIntent {
    App,
    Calculator,
    SystemAction,
    FileSearch,
    Clipboard,
    Unknown
};

struct IntentMap {
    PrimaryIntent primary{PrimaryIntent::App};
    std::unordered_map<std::string, float> confidences; // e.g. {"app": 0.94, "calc": 0.10}
};

class IntentDetector {
public:
    static IntentDetector& instance() noexcept;

    IntentMap classify(const NormalizedQuery& query) const;

private:
    IntentDetector() = default;
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_INTENT_DETECTOR_HPP
