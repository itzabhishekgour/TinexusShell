#pragma once

#include <txui/widgets/Widget.hpp>
#include <txui/render/Painter.hpp>
#include <txui/input/Event.hpp>
#include <vector>
#include <string>
#include <functional>

namespace tinexus::shell {

struct ShellNotificationItem {
    uint32_t id{0};
    std::string app_name;
    std::string title;
    std::string body;
    std::string time_ago;
    txui::Color icon_col;
    int urgency{1}; // 0: low, 1: normal, 2: critical
};

class NotificationFlyoutWidget : public txui::Widget {
public:
    std::function<void()> on_clear_all;

    NotificationFlyoutWidget();

    void set_notifications(const std::vector<ShellNotificationItem>& items) {
        m_notifications = items;
        mark_needs_layout();
        mark_needs_paint();
    }

    void clear_notifications() {
        m_notifications.clear();
        mark_needs_layout();
        mark_needs_paint();
    }

    [[nodiscard]] const std::vector<ShellNotificationItem>& notifications() const noexcept {
        return m_notifications;
    }

    [[nodiscard]] double calculate_height() const noexcept;

    txui::Size measure_override(const txui::Constraints& constraints) noexcept override;
    void layout_override(const txui::Rect& frame) noexcept override;
    void paint_override(txui::Painter& painter) const noexcept override;
    bool handle_event(const txui::Event& event) noexcept override;

private:
    std::vector<ShellNotificationItem> m_notifications;
    int m_hovered_index{-1};
    bool m_hover_clear{false};
};

} // namespace tinexus::shell
