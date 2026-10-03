#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/widgets/TextInput.hpp>
#include <txui/math/Rect.hpp>
#include <string>

namespace tinexus::lock {

class LockWidget : public txui::Widget {
public:
    LockWidget();
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
    [[nodiscard]] bool is_shaking() const noexcept { return m_shaking; }

    // Called from the main loop timer to advance the shake animation one frame.
    // Returns true while the animation is still running.
    bool advance_shake() noexcept;

protected:
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;

private:
    std::string m_password;
    txui::Ref<txui::TextInput> m_input;

    bool m_shaking{false};
    int  m_shake_offset{0};   // pixels of horizontal displacement, updated by advance_shake()
    int  m_shake_frame{0};    // current frame within the 8-frame shake sequence
    int  m_lockout_seconds{0};
    bool m_caret_visible{true};
};

} // namespace tinexus::lock
