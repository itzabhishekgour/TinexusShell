#include "searchd/intent_detector.hpp"
#include <algorithm>

namespace tinexus::searchd {

IntentDetector& IntentDetector::instance() noexcept {
    static IntentDetector s_instance;
    return s_instance;
}

static bool is_math_expression(const std::string& str) {
    if (str.empty()) return false;
    bool has_operator = false;
    for (char c : str) {
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%') {
            has_operator = true;
        } else if (!std::isdigit(c) && c != '.' && c != '(' && c != ')' && c != ' ') {
            return false;
        }
    }
    return has_operator;
}

IntentMap IntentDetector::classify(const NormalizedQuery& query) const {
    IntentMap map;

    // Handle explicit prefix tag
    if (query.has_prefix) {
        if (query.prefix_tag == "calc") {
            map.primary = PrimaryIntent::Calculator;
            map.confidences["calc"] = 1.0f;
            return map;
        } else if (query.prefix_tag == "sys") {
            map.primary = PrimaryIntent::SystemAction;
            map.confidences["sys"] = 1.0f;
            return map;
        } else if (query.prefix_tag == "app") {
            map.primary = PrimaryIntent::App;
            map.confidences["app"] = 1.0f;
            return map;
        } else if (query.prefix_tag == "file") {
            map.primary = PrimaryIntent::FileSearch;
            map.confidences["file"] = 1.0f;
            return map;
        } else if (query.prefix_tag == "clip") {
            map.primary = PrimaryIntent::Clipboard;
            map.confidences["clip"] = 1.0f;
            return map;
        }
    }

    const std::string& q = query.clean_query;

    if (is_math_expression(q)) {
        map.primary = PrimaryIntent::Calculator;
        map.confidences["calc"] = 0.98f;
        map.confidences["app"] = 0.20f;
        return map;
    }

    if (q == "lock" || q == "sleep" || q == "restart" || q == "reboot" || q == "shutdown" || q == "poweroff" || q == "logout") {
        map.primary = PrimaryIntent::SystemAction;
        map.confidences["sys"] = 0.98f;
        map.confidences["app"] = 0.40f;
        return map;
    }

    // Default intent is App search
    map.primary = PrimaryIntent::App;
    map.confidences["app"] = 0.90f;
    map.confidences["sys"] = 0.30f;
    map.confidences["file"] = 0.20f;

    return map;
}

} // namespace tinexus::searchd
