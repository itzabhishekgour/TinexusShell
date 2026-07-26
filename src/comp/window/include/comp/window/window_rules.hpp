#ifndef TINEXUS_COMP_WINDOW_RULES_HPP
#define TINEXUS_COMP_WINDOW_RULES_HPP

#include <string>
#include <vector>
#include <optional>

namespace tinexus::comp {

struct WindowRule {
    std::string app_id;
    std::optional<std::string> workspace_name;
    std::optional<bool> floating;
    std::optional<float> opacity;
    std::optional<bool> fullscreen;
};

class WindowRulesEngine {
public:
    static WindowRulesEngine& instance() noexcept;

    WindowRulesEngine() = default;
    ~WindowRulesEngine() = default;

    void add_rule(const WindowRule& rule);
    std::optional<WindowRule> match_rule(const std::string& app_id) const;

private:
    std::vector<WindowRule> m_rules;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_WINDOW_RULES_HPP
