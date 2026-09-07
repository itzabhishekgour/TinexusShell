#ifndef TINEXUS_NOTIFICATION_STACK_WIDGET_HPP
#define TINEXUS_NOTIFICATION_STACK_WIDGET_HPP

#include "notifications/NotificationBubble.hpp"
#include "notifications/notification_manager.hpp"
#include "notifications/dbus_server.hpp"
#include <txui/widgets/Widget.hpp>
#include <vector>
#include <chrono>
#include <functional>

namespace tinexus::notifications {

class NotificationStackWidget : public txui::Widget {
public:
    NotificationStackWidget();

    void add_bubble(const NotificationItem& item);
    void sync_active_notifications();
    void update(std::chrono::milliseconds delta_time);

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

    [[nodiscard]] double calculate_stack_height() const noexcept;
    [[nodiscard]] bool has_active_bubbles() const noexcept;
    [[nodiscard]] const std::vector<txui::Ref<NotificationBubble>>& bubbles() const noexcept { return m_bubbles; }

    void set_on_height_changed(std::function<void(uint32_t new_height)> cb) { m_on_height_changed = std::move(cb); }

private:
    std::vector<txui::Ref<NotificationBubble>> m_bubbles;
    uint32_t m_last_height{0};
    std::function<void(uint32_t)> m_on_height_changed;
};

} // namespace tinexus::notifications

#endif // TINEXUS_NOTIFICATION_STACK_WIDGET_HPP
