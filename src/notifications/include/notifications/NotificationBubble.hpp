#ifndef TINEXUS_NOTIFICATION_BUBBLE_HPP
#define TINEXUS_NOTIFICATION_BUBBLE_HPP

#include "notifications/notification_item.hpp"
#include <txui/widgets/Widget.hpp>
#include <txui/widgets/Button.hpp>
#include <txui/render/Painter.hpp>
#include <txui/math/Rect.hpp>
#include <txui/math/Point.hpp>
#include <chrono>
#include <string>
#include <vector>
#include <functional>

namespace tinexus::notifications {

enum class BubbleState {
    Hidden,
    SlidingIn,
    Visible,
    SlidingOut
};

class NotificationBubble : public txui::Widget {
public:
    explicit NotificationBubble(const NotificationItem& item);

    // Updates physics/timer. Returns true while active, false when completely hidden.
    bool update(std::chrono::milliseconds delta_time);

    void handle_drag(double delta_x);
    void handle_release(double velocity_x);
    void dismiss();

    // txui::Widget overrides
    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

    // Direct painter helper for standalone/stacked coordinate painting
    void paint_at(txui::Painter& painter, double y, double width, double pointer_x = -1.0, double pointer_y = -1.0) const noexcept;

    // Hit testing (compatible with legacy callers & tests)
    [[nodiscard]] bool hit_test_close(double local_x, double local_y, double width) const noexcept;
    [[nodiscard]] int hit_test_action(double local_x, double local_y, double width) const noexcept;

    [[nodiscard]] double height() const noexcept;
    [[nodiscard]] double offset_x() const noexcept { return m_offset_x; }
    [[nodiscard]] double opacity() const noexcept { return m_opacity; }
    [[nodiscard]] BubbleState state() const noexcept { return m_state; }
    [[nodiscard]] const NotificationItem& item() const noexcept { return m_item; }
    [[nodiscard]] NotificationId id() const noexcept { return m_item.id; }

    void set_on_dismiss(std::function<void()> cb) { m_on_dismiss = std::move(cb); }
    void set_on_action(std::function<void(const std::string& action_key)> cb) { m_on_action = std::move(cb); }

private:
    NotificationItem m_item;
    BubbleState m_state{BubbleState::SlidingIn};

    double m_offset_x{420.0};
    double m_opacity{0.0};
    double m_display_time_ms{0.0};
    double m_total_time_ms{6000.0};

    static constexpr double SLIDE_SPEED = 1400.0;
    static constexpr double SWIPE_THRESHOLD = 150.0;

    txui::Ref<txui::Button> m_close_btn;
    std::vector<txui::Ref<txui::Button>> m_action_buttons;

    std::function<void()> m_on_dismiss;
    std::function<void(const std::string& action_key)> m_on_action;
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATION_BUBBLE_HPP
