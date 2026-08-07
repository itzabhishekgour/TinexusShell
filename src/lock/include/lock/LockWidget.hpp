#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/math/Rect.hpp>
#include <string>

namespace tinexus::lock {

class LockWidget : public txui::Widget {
public:
    LockWidget() = default;
    ~LockWidget() override = default;

    void add_password_char(char c);
    void remove_password_char();
    void clear_password();
    [[nodiscard]] std::string get_password() const { return m_password; }

    void trigger_shake_animation();
    void set_lockout(int seconds);
    void toggle_caret() noexcept { m_caret_visible = !m_caret_visible; mark_needs_paint(); }
    void set_caret_visible(bool visible) noexcept { m_caret_visible = visible; mark_needs_paint(); }
    [[nodiscard]] bool is_caret_visible() const noexcept { return m_caret_visible; }

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

private:
    std::string m_password;
    bool m_shaking{false};
    int m_shake_offset{0};
    int m_lockout_seconds{0};
    bool m_caret_visible{true};
};

} // namespace tinexus::lock
