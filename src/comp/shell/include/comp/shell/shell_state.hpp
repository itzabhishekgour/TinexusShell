#ifndef TINEXUS_COMP_SHELL_STATE_HPP
#define TINEXUS_COMP_SHELL_STATE_HPP

#include <string>

namespace tinexus::comp {

class ShellState {
public:
    static ShellState& instance() noexcept;

    ShellState() = default;
    ~ShellState() = default;

    [[nodiscard]] bool launcher_visible() const noexcept;
    void set_launcher_visible(bool visible);

    [[nodiscard]] bool lockscreen_active() const noexcept;
    void set_lockscreen_active(bool active);

    [[nodiscard]] bool overview_active() const noexcept;
    void set_overview_active(bool active);

private:
    bool m_launcher_visible{false};
    bool m_lockscreen_active{false};
    bool m_overview_active{false};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SHELL_STATE_HPP
