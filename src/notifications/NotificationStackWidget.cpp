#include "notifications/NotificationStackWidget.hpp"
#include <algorithm>

namespace tinexus::notifications {

NotificationStackWidget::NotificationStackWidget() = default;

void NotificationStackWidget::add_bubble(const NotificationItem& item) {
    for (const auto& b : m_bubbles) {
        if (b->id() == item.id) return;
    }

    auto bubble = txui::make_ref<NotificationBubble>(item);
    bubble->set_on_dismiss([this, id = item.id]() {
        DBusServer::instance().emit_notification_closed(
            static_cast<uint32_t>(id),
            static_cast<uint32_t>(ClosedReason::Dismissed)
        );
        NotificationManager::instance().close_notification(id, ClosedReason::Dismissed);
    });

    bubble->set_on_action([this, id = item.id](const std::string& key) {
        DBusServer::instance().emit_action_invoked(
            static_cast<uint32_t>(id),
            key
        );
    });

    m_bubbles.push_back(bubble);

    // Limit active visible stack to 3 notifications
    if (m_bubbles.size() > 3) {
        for (size_t i = 3; i < m_bubbles.size(); ++i) {
            m_bubbles[i]->dismiss();
        }
    }

    mark_needs_layout();
    mark_needs_paint();
}

void NotificationStackWidget::sync_active_notifications() {
    auto active_items = NotificationManager::instance().active_queue();
    for (const auto& item : active_items) {
        bool exists = false;
        for (const auto& b : m_bubbles) {
            if (b->id() == item.id) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            add_bubble(item);
        }
    }
}

void NotificationStackWidget::update(std::chrono::milliseconds delta_time) {
    sync_active_notifications();

    for (auto it = m_bubbles.begin(); it != m_bubbles.end();) {
        bool active = (*it)->update(delta_time);
        if (!active && (*it)->state() == BubbleState::Hidden) {
            DBusServer::instance().emit_notification_closed(
                static_cast<uint32_t>((*it)->id()),
                static_cast<uint32_t>(ClosedReason::Expired)
            );
            NotificationManager::instance().close_notification(
                (*it)->id(), ClosedReason::Expired
            );
            it = m_bubbles.erase(it);
        } else {
            ++it;
        }
    }

    uint32_t current_h = static_cast<uint32_t>(calculate_stack_height());
    if (current_h != m_last_height) {
        m_last_height = current_h;
        if (m_on_height_changed) {
            m_on_height_changed(m_last_height);
        }
    }

    mark_needs_layout();
    mark_needs_paint();
}

double NotificationStackWidget::calculate_stack_height() const noexcept {
    if (m_bubbles.empty()) return 0.0;

    double total = 8.0;
    for (const auto& b : m_bubbles) {
        total += b->height() + 8.0;
    }
    return total;
}

bool NotificationStackWidget::has_active_bubbles() const noexcept {
    return !m_bubbles.empty();
}

std::vector<txui::Rect> NotificationStackWidget::get_input_rects() const noexcept {
    std::vector<txui::Rect> rects;
    rects.reserve(m_bubbles.size());
    for (const auto& b : m_bubbles) {
        if (b && b->state() != BubbleState::Hidden) {
            const auto& f = b->frame();
            if (f.width() > 0.0 && f.height() > 0.0) {
                rects.push_back(f);
            }
        }
    }
    return rects;
}

txui::Size NotificationStackWidget::measure_override(const txui::Constraints& constraints) noexcept {
    double w = constraints.max_width;
    if (w <= 0.0) w = 420.0;
    return txui::Size(w, calculate_stack_height());
}

void NotificationStackWidget::layout_override(const txui::Rect& f) noexcept {
    double cy = f.y() + 4.0;
    for (auto& b : m_bubbles) {
        double bh = b->height();
        b->layout(txui::Rect(f.x() + 4.0, cy, f.width() - 8.0, bh));
        cy += bh + 8.0;
    }
}

void NotificationStackWidget::paint_override(txui::Painter& painter) const noexcept {
    for (const auto& b : m_bubbles) {
        b->paint(painter);
    }
}

bool NotificationStackWidget::handle_event(const txui::Event& event) noexcept {
    for (auto& b : m_bubbles) {
        if (b->handle_event(event)) {
            return true;
        }
    }
    return false;
}

} // namespace tinexus::notifications
