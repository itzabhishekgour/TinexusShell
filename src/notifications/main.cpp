#include "notifications/notification_server.hpp"
#include "notifications/notification_manager.hpp"
#include "notifications/NotificationBubble.hpp"
#include "notifications/dbus_server.hpp"

#include <txui/wayland/WaylandConnection.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <txui/theme/Theme.hpp>
#include <common/logger.hpp>

#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <wayland-client.h>
#include <csignal>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <poll.h>
#include <linux/input-event-codes.h>

using namespace tinexus;

static zwlr_layer_shell_v1* g_layer_shell = nullptr;
static wl_seat* g_seat = nullptr;
static wl_pointer* g_pointer = nullptr;
static std::atomic<bool> g_running{true};

static double g_pointer_x = -1.0;
static double g_pointer_y = -1.0;
static bool g_pointer_clicked = false;
static uint32_t g_current_width = 420;
static uint32_t g_current_height = 100;

static void pointer_handle_enter(void*, struct wl_pointer*, uint32_t, struct wl_surface*, wl_fixed_t sx, wl_fixed_t sy) {
    g_pointer_x = wl_fixed_to_double(sx);
    g_pointer_y = wl_fixed_to_double(sy);
}

static void pointer_handle_leave(void*, struct wl_pointer*, uint32_t, struct wl_surface*) {
    g_pointer_x = -1.0;
    g_pointer_y = -1.0;
}

static void pointer_handle_motion(void*, struct wl_pointer*, uint32_t, wl_fixed_t sx, wl_fixed_t sy) {
    g_pointer_x = wl_fixed_to_double(sx);
    g_pointer_y = wl_fixed_to_double(sy);
}

static void pointer_handle_button(void*, struct wl_pointer*, uint32_t, uint32_t, uint32_t button, uint32_t state) {
    if (state == WL_POINTER_BUTTON_STATE_PRESSED && (button == BTN_LEFT || button == 0x110)) {
        g_pointer_clicked = true;
    }
}

static void pointer_handle_axis(void*, struct wl_pointer*, uint32_t, uint32_t, wl_fixed_t) {}
static void pointer_handle_frame(void*, struct wl_pointer*) {}
static void pointer_handle_axis_source(void*, struct wl_pointer*, uint32_t) {}
static void pointer_handle_axis_stop(void*, struct wl_pointer*, uint32_t, uint32_t) {}
static void pointer_handle_axis_discrete(void*, struct wl_pointer*, uint32_t, int32_t) {}
static void pointer_handle_axis_value120(void*, struct wl_pointer*, uint32_t, int32_t) {}
static void pointer_handle_axis_relative_direction(void*, struct wl_pointer*, uint32_t, uint32_t) {}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_handle_enter,
    .leave = pointer_handle_leave,
    .motion = pointer_handle_motion,
    .button = pointer_handle_button,
    .axis = pointer_handle_axis,
    .frame = pointer_handle_frame,
    .axis_source = pointer_handle_axis_source,
    .axis_stop = pointer_handle_axis_stop,
    .axis_discrete = pointer_handle_axis_discrete,
    .axis_value120 = pointer_handle_axis_value120,
    .axis_relative_direction = pointer_handle_axis_relative_direction,
};

static void seat_handle_capabilities(void*, struct wl_seat* seat, uint32_t caps) {
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !g_pointer) {
        g_pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(g_pointer, &pointer_listener, nullptr);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && g_pointer) {
        wl_pointer_destroy(g_pointer);
        g_pointer = nullptr;
    }
}

static void seat_handle_name(void*, struct wl_seat*, const char*) {}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_handle_capabilities,
    .name = seat_handle_name,
};

static void registry_handle_global(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (std::string_view(interface) == zwlr_layer_shell_v1_interface.name) {
        g_layer_shell = static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version >= 4 ? 4 : version));
        log::info("tinexus-notifications: Bound zwlr_layer_shell_v1 global");
    } else if (std::string_view(interface) == wl_seat_interface.name && !g_seat) {
        g_seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, 5));
        wl_seat_add_listener(g_seat, &seat_listener, nullptr);
    }
}

static void registry_handle_global_remove(void*, struct wl_registry*, uint32_t) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void layer_surface_configure(void*, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t w, uint32_t h) {
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    if (w > 0) g_current_width = w;
    if (h > 0) g_current_height = h;
}

static void layer_surface_closed(void*, struct zwlr_layer_surface_v1*) {
    g_running = false;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

void signal_handler(int) {
    g_running = false;
}

int main() {
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);

    log::set_component_name("notifications");
    log::info("[Notifications] Tinexus Notification Center Daemon starting...");

    if (!notifications::DBusServer::instance().start()) {
        log::error("[Notifications] Failed to start DBus Server!");
    }

    auto conn_opt = txui::wayland::WaylandConnection::connect();
    if (!conn_opt) {
        log::error("[Notifications] Failed to connect to Wayland display!");
        return 1;
    }
    auto connection = std::move(*conn_opt);

    wl_registry* reg = wl_display_get_registry(connection.display());
    wl_registry_add_listener(reg, &registry_listener, nullptr);
    wl_display_roundtrip(connection.display());

    if (!g_layer_shell) {
        log::error("[Notifications] Compositor does not support zwlr_layer_shell_v1!");
        return 1;
    }

    // Enqueue initial welcome notification through standard notification manager
    notifications::NotificationServer::instance().notify(
        "Tinexus System",
        0,
        "tinexus",
        "Welcome to Tinexus OS",
        "Press Ctrl+K to open Pulse launcher",
        {},
        notifications::Urgency::Normal,
        8000
    );

    std::vector<notifications::NotificationBubble> bubbles;
    std::unique_ptr<txui::WaylandRenderTarget> render_target;
    struct zwlr_layer_surface_v1* layer_surface = nullptr;
    wl_surface* raw_surface = nullptr;

    auto recreate_or_resize_surface = [&](uint32_t target_w, uint32_t target_h) {
        if (!render_target) {
            auto opt = txui::WaylandRenderTarget::create(connection, target_w, target_h);
            if (!opt) {
                log::error("[Notifications] Failed to create WaylandRenderTarget!");
                return false;
            }
            render_target = std::make_unique<txui::WaylandRenderTarget>(std::move(*opt));
            raw_surface = render_target->surface().surface();

            layer_surface = zwlr_layer_shell_v1_get_layer_surface(
                g_layer_shell, raw_surface, nullptr,
                ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "tinexus-notifications");

            zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, nullptr);
            zwlr_layer_surface_v1_set_size(layer_surface, target_w, target_h);
            zwlr_layer_surface_v1_set_margin(layer_surface, 16, 16, 0, 0);
            zwlr_layer_surface_v1_set_anchor(layer_surface,
                ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
            zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);

            wl_surface_commit(raw_surface);
            connection.flush();
            connection.roundtrip();
            g_current_width = target_w;
            g_current_height = target_h;
        } else if (g_current_width != target_w || g_current_height != target_h) {
            render_target->resize(target_w, target_h);
            zwlr_layer_surface_v1_set_size(layer_surface, target_w, target_h);
            wl_surface_commit(raw_surface);
            connection.flush();
            g_current_width = target_w;
            g_current_height = target_h;
        }
        return true;
    };

    txui::CommandBuffer cmd_buf;
    txui::PixmanBackend backend;

    int wl_fd = wl_display_get_fd(connection.display());
    int bus_fd = -1;
    if (notifications::DBusServer::instance().bus()) {
        bus_fd = sd_bus_get_fd(notifications::DBusServer::instance().bus());
    }

    auto last_time = std::chrono::steady_clock::now();

    while (g_running) {
        while (wl_display_prepare_read(connection.display()) != 0) {
            wl_display_dispatch_pending(connection.display());
        }
        wl_display_flush(connection.display());

        struct pollfd fds[2] = {};
        fds[0].fd = wl_fd;
        fds[0].events = POLLIN;
        int num_fds = 1;

        if (bus_fd >= 0) {
            fds[1].fd = bus_fd;
            fds[1].events = POLLIN;
            num_fds = 2;
        }

        int poll_timeout_ms = bubbles.empty() ? 100 : 16; // 60 FPS when active, 100ms when idle
        int ret = poll(fds, static_cast<nfds_t>(num_fds), poll_timeout_ms);

        if (ret > 0) {
            if (fds[0].revents & POLLIN) {
                wl_display_read_events(connection.display());
                wl_display_dispatch_pending(connection.display());
            } else {
                wl_display_cancel_read(connection.display());
            }

            if (num_fds == 2 && (fds[1].revents & POLLIN)) {
                while (sd_bus_process(notifications::DBusServer::instance().bus(), nullptr) > 0) {}
            }
        } else {
            wl_display_cancel_read(connection.display());
        }

        // 1. Sync active notifications from manager
        auto active_items = notifications::NotificationManager::instance().active_queue();
        for (const auto& item : active_items) {
            bool exists = false;
            for (auto& b : bubbles) {
                if (b.id() == item.id) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                bubbles.emplace_back(item);
            }
        }

        // 2. Update physics & timers for bubbles
        auto now = std::chrono::steady_clock::now();
        auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_time);
        last_time = now;

        for (auto it = bubbles.begin(); it != bubbles.end();) {
            bool active = it->update(delta_ms);
            if (!active && it->state() == notifications::BubbleState::Hidden) {
                // Emitted closed signal to D-Bus
                notifications::DBusServer::instance().emit_notification_closed(
                    static_cast<uint32_t>(it->id()),
                    static_cast<uint32_t>(notifications::ClosedReason::Expired)
                );
                notifications::NotificationManager::instance().close_notification(
                    it->id(), notifications::ClosedReason::Expired
                );
                it = bubbles.erase(it);
            } else {
                ++it;
            }
        }

        // Limit stack to top 3 visible notifications
        if (bubbles.size() > 3) {
            for (size_t i = 3; i < bubbles.size(); ++i) {
                bubbles[i].dismiss();
            }
        }

        // 3. Handle pointer interactions (Clicks)
        if (g_pointer_clicked) {
            g_pointer_clicked = false;
            double current_y = 0.0;
            for (auto& b : bubbles) {
                double bh = b.height();
                if (g_pointer_y >= current_y && g_pointer_y <= current_y + bh) {
                    double local_x = g_pointer_x;
                    double local_y = g_pointer_y - current_y;

                    // Check close button hit
                    if (b.hit_test_close(local_x, local_y, g_current_width)) {
                        b.dismiss();
                        notifications::DBusServer::instance().emit_notification_closed(
                            static_cast<uint32_t>(b.id()),
                            static_cast<uint32_t>(notifications::ClosedReason::Dismissed)
                        );
                        notifications::NotificationManager::instance().close_notification(
                            b.id(), notifications::ClosedReason::Dismissed
                        );
                        break;
                    }

                    // Check action buttons hit
                    int act_idx = b.hit_test_action(local_x, local_y, g_current_width);
                    if (act_idx >= 0 && static_cast<size_t>(act_idx) < b.item().actions.size()) {
                        notifications::DBusServer::instance().emit_action_invoked(
                            static_cast<uint32_t>(b.id()),
                            b.item().actions[static_cast<size_t>(act_idx)].action_key
                        );
                        b.dismiss();
                        break;
                    }

                    // Clicking the body also dismisses it smoothly
                    b.dismiss();
                    break;
                }
                current_y += bh + 8.0;
            }
        }

        // 4. Calculate total stack dimensions
        uint32_t target_w = 420;
        uint32_t target_h = 0;
        for (const auto& b : bubbles) {
            target_h += static_cast<uint32_t>(b.height()) + 8;
        }

        // 5. Render or unmap surface
        if (bubbles.empty() || target_h == 0) {
            if (layer_surface && raw_surface) {
                wl_surface_attach(raw_surface, nullptr, 0, 0);
                wl_surface_commit(raw_surface);
                connection.flush();
            }
            continue;
        }

        if (!recreate_or_resize_surface(target_w, target_h)) {
            continue;
        }

        // Paint frame
        txui::Painter painter(cmd_buf);
        painter.begin_frame();
        // Clear surface to transparent
        painter.clear(txui::Color::transparent());

        double render_y = 0.0;
        for (const auto& b : bubbles) {
            b.paint(painter, render_y, static_cast<double>(target_w), g_pointer_x, g_pointer_y);
            render_y += b.height() + 8.0;
        }

        painter.end_frame();

        backend.execute(cmd_buf, *render_target);
        cmd_buf.clear();
        render_target->present();
        connection.flush();
    }

    if (g_pointer) wl_pointer_destroy(g_pointer);
    if (g_seat) wl_seat_destroy(g_seat);
    if (layer_surface) zwlr_layer_surface_v1_destroy(layer_surface);
    return 0;
}
