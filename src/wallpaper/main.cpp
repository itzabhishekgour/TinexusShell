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
#include <fstream>
#include <cerrno>
#include <cstring>
#include <poll.h>

using namespace tinexus;
namespace fs = std::filesystem;

static zwlr_layer_shell_v1* g_layer_shell = nullptr;
static std::atomic<bool> g_running{true};
static std::atomic<bool> g_reload_requested{false};

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

void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        g_reload_requested = true;
    } else {
        g_running = false;
    }
}

static std::string get_active_wallpaper_path() {
    auto check_path = [](std::string p) -> std::string {
        p.erase(0, p.find_first_not_of(" \t\r\n\"'"));
        p.erase(p.find_last_not_of(" \t\r\n\"'") + 1);
        if (p.empty()) return "";
        if (fs::exists(p)) return p;
        // Check /usr/share/backgrounds/ fallback with basename
        std::string fname = fs::path(p).filename().string();
        std::string bg_candidate = "/usr/share/backgrounds/" + fname;
        if (fs::exists(bg_candidate)) return bg_candidate;
        return "";
    };

    // 1. Check /tmp/current_wallpaper (universal IPC across users)
    if (fs::exists("/tmp/current_wallpaper")) {
        std::ifstream f("/tmp/current_wallpaper");
        std::string p;
        if (std::getline(f, p)) {
            std::string valid = check_path(p);
            if (!valid.empty()) return valid;
        }
    }

    // 2. Check XDG_RUNTIME_DIR and standard user runtime dirs
    const char* xdg_run = std::getenv("XDG_RUNTIME_DIR");
    std::string run_path = xdg_run ? (std::string(xdg_run) + "/tinexus/current_wallpaper") : "";
    if (!run_path.empty() && fs::exists(run_path)) {
        std::ifstream f(run_path);
        std::string p;
        if (std::getline(f, p)) {
            std::string valid = check_path(p);
            if (!valid.empty()) return valid;
        }
    }
    for (const auto& rdir : {"/run/user/1000/tinexus/current_wallpaper", "/run/user/0/tinexus/current_wallpaper"}) {
        if (fs::exists(rdir)) {
            std::ifstream f(rdir);
            std::string p;
            if (std::getline(f, p)) {
                std::string valid = check_path(p);
                if (!valid.empty()) return valid;
            }
        }
    }

    // 3. Check ~/.config/tinexus/settings.toml for wallpaper_path
    const char* home = std::getenv("HOME");
    std::string cfg_path = home ? (std::string(home) + "/.config/tinexus/settings.toml") : "";
    if (!cfg_path.empty() && fs::exists(cfg_path)) {
        std::ifstream f(cfg_path);
        std::string line;
        while (std::getline(f, line)) {
            if (line.starts_with("wallpaper_path")) {
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::string p = line.substr(pos + 1);
                    std::string valid = check_path(p);
                    if (!valid.empty()) return valid;
                }
            }
        }
    }

    // 4. Default candidates
    for (const auto& c : {"/usr/share/backgrounds/tinexus-default.jpg",
                         "/usr/share/backgrounds/tinexus-os-primary.jpg",
                         "/usr/share/backgrounds/sunset-gradient.png",
                         "/usr/share/backgrounds/emerald-matrix.png",
                         "/home/tinexus/Pictures/tinexus-default.jpg",
                         "assets/wallpaper/tinexus-default.jpg"}) {
        if (fs::exists(c)) return c;
    }
    return "";
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // No SA_RESTART: interrupts poll() immediately on SIGUSR1
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGUSR1, &sa, nullptr);

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

    wallpaper::ImageProvider provider;

    auto render_and_commit = [&](uint32_t width, uint32_t height) {
        std::string path = get_active_wallpaper_path();
        if (!path.empty() && provider.load(path)) {
            log::info("[Wallpaper] Loaded wallpaper image from '{}'", path);
        } else {
            log::warn("[Wallpaper] Custom wallpaper image not found at '{}', using procedural fallback.", path);
        }

        wallpaper::WallpaperBuffer buf = provider.render_buffer(width, height);
        if (render_target->data() && !buf.pixels.empty()) {
            // Synchronize both double-buffers in the swapchain
            std::copy(buf.pixels.begin(), buf.pixels.end(), render_target->data());
            render_target->present();
            std::copy(buf.pixels.begin(), buf.pixels.end(), render_target->data());
            render_target->present();
            connection.flush();
            connection.roundtrip();
            log::info("[Wallpaper] Wallpaper committed to BACKGROUND layer surface successfully.");
        }
    };

    // Initial render
    render_and_commit(w, h);
    std::string last_loaded_path = get_active_wallpaper_path();

    // Resilient Wayland event loop: non-blocking poll with 250ms timeout for instant response
    while (g_running) {
        std::string current_path = get_active_wallpaper_path();
        if (g_reload_requested || (!current_path.empty() && current_path != last_loaded_path)) {
            g_reload_requested = false;
            last_loaded_path = current_path;
            log::info("[Wallpaper] Reloading wallpaper (requested or changed: '{}')", current_path);
            render_and_commit(w, h);
        }

        while (wl_display_prepare_read(connection.display()) != 0) {
            wl_display_dispatch_pending(connection.display());
        }
        connection.flush();

        struct pollfd pfd{};
        pfd.fd = wl_display_get_fd(connection.display());
        pfd.events = POLLIN;
        int ret = poll(&pfd, 1, 250); // 250ms responsive timeout
        if (ret > 0) {
            wl_display_read_events(connection.display());
            wl_display_dispatch_pending(connection.display());
        } else {
            wl_display_cancel_read(connection.display());
            if (ret < 0 && errno != EINTR) {
                log::error("[Wallpaper] poll error: {}", std::strerror(errno));
                break;
            }
        }
    }

    if (layer_surface) zwlr_layer_surface_v1_destroy(layer_surface);
    log::info("[Wallpaper] Exiting wallpaper daemon.");
    return 0;
}
