#include "wallpaper/wallpaper_provider.hpp"
#include <txui/wayland/WaylandConnection.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <common/logger.hpp>

#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <wayland-client.h>
#include <csignal>
#include <atomic>
#include <thread>
#include <filesystem>

using namespace tinexus;
namespace fs = std::filesystem;

static zwlr_layer_shell_v1* g_layer_shell = nullptr;
static std::atomic<bool> g_running{true};

static void registry_handle_global(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
    if (std::string_view(interface) == zwlr_layer_shell_v1_interface.name) {
        g_layer_shell = static_cast<zwlr_layer_shell_v1*>(wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, version >= 4 ? 4 : version));
        log::info("tinexus-wallpaper: Bound zwlr_layer_shell_v1 global");
    }
}

static void registry_handle_global_remove(void*, struct wl_registry*, uint32_t) {}

static const struct wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

static bool g_configured = false;
static uint32_t g_configured_w = 0;
static uint32_t g_configured_h = 0;

static void layer_surface_configure(void* data, struct zwlr_layer_surface_v1* surface, uint32_t serial, uint32_t width, uint32_t height) {
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    g_configured = true;
    if (width > 0 && height > 0) {
        g_configured_w = width;
        g_configured_h = height;
    }
    log::info("tinexus-wallpaper: Configured surface size {}x{}", width, height);
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1* surface) {
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

    log::set_component_name("wallpaper");
    log::info("[Wallpaper] Tinexus Wallpaper Daemon starting...");

    // Connect to Wayland display
    auto conn_opt = txui::wayland::WaylandConnection::connect();
    if (!conn_opt) {
        log::error("[Wallpaper] Failed to connect to Wayland display!");
        return 1;
    }
    auto connection = std::move(*conn_opt);

    wl_registry* reg = wl_display_get_registry(connection.display());
    wl_registry_add_listener(reg, &registry_listener, nullptr);
    wl_display_roundtrip(connection.display());

    if (!g_layer_shell) {
        log::error("[Wallpaper] Compositor does not support zwlr_layer_shell_v1!");
        return 1;
    }

    uint32_t w = 800;
    uint32_t h = 600;

    auto target_opt = txui::WaylandRenderTarget::create(connection, w, h);
    if (!target_opt) {
        log::error("[Wallpaper] Failed to create WaylandRenderTarget!");
        return 1;
    }
    auto render_target = std::make_unique<txui::WaylandRenderTarget>(std::move(*target_opt));

    wl_surface* raw_surface = render_target->surface().surface();

    struct zwlr_layer_surface_v1* layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        g_layer_shell, raw_surface, nullptr,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "tinexus-wallpaper");

    zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, nullptr);
    zwlr_layer_surface_v1_set_size(layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_anchor(layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);

    wl_surface_commit(raw_surface);
    connection.flush();
    connection.roundtrip();

    // Wait for the initial configure event before rendering and attaching a buffer!
    while (!g_configured && g_running) {
        if (wl_display_dispatch(connection.display()) == -1) {
            break;
        }
    }

    if (!g_running) {
        return 0;
    }

    if (g_configured_w > 0 && g_configured_h > 0 && (g_configured_w != w || g_configured_h != h)) {
        render_target->resize(g_configured_w, g_configured_h);
        w = g_configured_w;
        h = g_configured_h;
    }

    // Try loading candidate wallpaper image paths
    std::vector<std::string> image_candidates = {
        "Temp/daniel-leone-v7daTKlZzaw-unsplash.jpg",
        "/usr/share/backgrounds/tinexus-default.jpg",
        "/usr/share/backgrounds/tinexus-default.png",
        "/usr/share/backgrounds/daniel-leone-v7daTKlZzaw-unsplash.jpg"
    };

    wallpaper::ImageProvider provider;
    bool loaded = false;
    for (const auto& path : image_candidates) {
        if (fs::exists(path) && provider.load(path)) {
            loaded = true;
            log::info("[Wallpaper] Using custom wallpaper image from '{}'", path);
            break;
        }
    }

    if (!loaded) {
        log::warn("[Wallpaper] Custom wallpaper image not found, using procedural fallback.");
    }

    wallpaper::WallpaperBuffer buf = provider.render_buffer(w, h);

    if (render_target->data() && !buf.pixels.empty()) {
        std::copy(buf.pixels.begin(), buf.pixels.end(), render_target->data());
        render_target->present();
        connection.flush();
        log::info("[Wallpaper] Custom wallpaper committed to BACKGROUND layer surface successfully.");
    }

    // Event loop
    while (g_running) {
        if (wl_display_dispatch(connection.display()) == -1) {
            break;
        }
    }

    if (layer_surface) zwlr_layer_surface_v1_destroy(layer_surface);
    log::info("[Wallpaper] Exiting wallpaper daemon.");
    return 0;
}
