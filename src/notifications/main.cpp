#include "notifications/notification_server.hpp"
#include <txui/wayland/WaylandConnection.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <txui/render/CommandBuffer.hpp>
#include <txui/render/Painter.hpp>
#include <common/logger.hpp>

#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <wayland-client.h>
#include <csignal>
#include <atomic>
#include <thread>
#include <chrono>

using namespace tinexus;

static zwlr_layer_shell_v1* g_layer_shell = nullptr;
static std::atomic<bool> g_running{true};

static void registry_handle_global(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (std::string_view(interface) == zwlr_layer_shell_v1_interface.name) {
        g_layer_shell = static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version >= 4 ? 4 : version));
        log::info("tinexus-notifications: Bound zwlr_layer_shell_v1 global");
    }
}

static void registry_handle_global_remove(void*, struct wl_registry*, uint32_t) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static void layer_surface_configure(void*, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t, uint32_t) {
    zwlr_layer_surface_v1_ack_configure(surface, serial);
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

    uint32_t w = 360;
    uint32_t h = 80;

    auto target_opt = txui::WaylandRenderTarget::create(connection, w, h);
    if (!target_opt) {
        log::error("[Notifications] Failed to create WaylandRenderTarget!");
        return 1;
    }
    auto render_target = std::make_unique<txui::WaylandRenderTarget>(std::move(*target_opt));

    wl_surface* raw_surface = render_target->surface().surface();

    struct zwlr_layer_surface_v1* layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        g_layer_shell, raw_surface, nullptr,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "tinexus-notifications");

    zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, nullptr);
    zwlr_layer_surface_v1_set_size(layer_surface, w, h);
    zwlr_layer_surface_v1_set_margin(layer_surface, 44, 16, 0, 0); // Top 44px, Right 16px
    zwlr_layer_surface_v1_set_anchor(layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);

    wl_surface_commit(raw_surface);
    connection.flush();
    connection.roundtrip();

    // Render Toast Banner
    txui::CommandBuffer cmd_buf;
    txui::Painter painter(cmd_buf);
    txui::PixmanBackend backend;

    painter.begin_frame();
    // Rounded toast container (#1E293B border, #0F172A body)
    painter.fill_rounded_rect(txui::Rect(0, 0, w, h), 8.0, txui::Color(30, 41, 59, 240));
    painter.fill_rounded_rect(txui::Rect(2, 2, w - 4, h - 4), 6.0, txui::Color(15, 23, 42, 240));
    // Accent left stripe (#3B82F6)
    painter.fill_rounded_rect(txui::Rect(4, 4, 6, h - 8), 3.0, txui::Color(59, 130, 246, 255));
    // Notification Text
    painter.draw_text(txui::Point(18, 14), "Welcome to Tinexus OS", txui::Color::white(), 1.0);
    painter.draw_text(txui::Point(18, 38), "Press Ctrl+K to open Launcher", txui::Color(148, 163, 184, 255), 1.0);
    painter.end_frame();

    backend.execute(cmd_buf, *render_target);
    cmd_buf.clear();
    render_target->present();
    connection.flush();

    log::info("[Notifications] Toast notification banner displayed at TOP-RIGHT.");

    // Display toast notification banner for 15 seconds, then hide
    auto start_time = std::chrono::steady_clock::now();
    while (g_running) {
        if (wl_display_dispatch_pending(connection.display()) == -1) break;

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count();
        if (elapsed > 15) {
            // Auto dismiss toast notification
            zwlr_layer_surface_v1_set_size(layer_surface, 0, 0);
            wl_surface_commit(raw_surface);
            connection.flush();
            log::info("[Notifications] Toast notification auto-dismissed.");
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    while (g_running) {
        if (wl_display_dispatch(connection.display()) == -1) break;
    }

    if (layer_surface) zwlr_layer_surface_v1_destroy(layer_surface);
    return 0;
}
