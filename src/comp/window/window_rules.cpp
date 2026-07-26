#include "comp/window/window_rules.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

WindowRulesEngine& WindowRulesEngine::instance() noexcept {
    static WindowRulesEngine s_instance;
    return s_instance;
}

void WindowRulesEngine::add_rule(const WindowRule& rule) {
    m_rules.push_back(rule);
    log::info("WindowRulesEngine: Registered rule for app_id '{}'", rule.app_id);
}

std::optional<WindowRule> WindowRulesEngine::match_rule(const std::string& app_id) const {
    for (const auto& r : m_rules) {
        if (r.app_id == app_id) {
            return r;
        }
    }
    return std::nullopt;
}

} // namespace tinexus::comp
