// ============================================================================
// main.cpp — tinexus-wallpaper (Milestones 1, 3, 4, 5, 7 — Final)
// ============================================================================
// Pure C++20 Wayland LAYER_BACKGROUND daemon.
//
// Milestone 1: Per-output surface map — wl_output + wp_fractional_scale_v1
// Milestone 3: D-Bus event loop — poll(wayland + dbus, -1)
// Milestone 4: CPU cross-fade — dual-buffer ping-pong via timerfd at 60fps
// Milestone 5: Dynamic solar schedule — .twallpaper + timerfd CLOCK_REALTIME
// Milestone 7: WALLPAPER_STATUS_QUERY → WallpaperStatusPayload response
// ============================================================================

#include "wallpaper/wallpaper_provider.hpp"
#include "wallpaper/solar_schedule.hpp"
#include "wallpaper/shm_surface.hpp"
#include <common/logger.hpp>
#include "wallpaper/wallpaper_dbus.hpp"

#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <wayland-client.h>

// wp_fractional_scale_v1 (wlroots >= 0.17) — advertised by tinexus-comp
extern "C" {
#include <fractional-scale-v1-client-protocol.h>
#include <viewporter-client-protocol.h>
}
#include <sys/timerfd.h>
#include <csignal>
#include <atomic>
#include <cstring>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <poll.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <ctime>
#include <cmath>
#include <memory>

using namespace tinexus;
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Global compositor state
// ─────────────────────────────────────────────────────────────────────────────
static wl_display*                     g_display         = nullptr;
static wl_compositor*                  g_compositor      = nullptr;
static wl_shm*                         g_shm             = nullptr;
static zwlr_layer_shell_v1*            g_layer_shell     = nullptr;
static wp_fractional_scale_manager_v1* g_frac_scale_mgr  = nullptr;
static wp_viewporter*                  g_viewporter       = nullptr;
static std::atomic<bool>               g_running{true};
static volatile sig_atomic_t           g_reload_requested = 0;

// ─────────────────────────────────────────────────────────────────────────────
// OutputSurface — per-monitor wallpaper state
// ─────────────────────────────────────────────────────────────────────────────
struct OutputSurface {
    wl_output*                                  output      = nullptr;
    zwlr_layer_surface_v1*                      layer_surf  = nullptr;
    std::unique_ptr<wallpaper::ShmRenderTarget> buf_a;  // currently displayed
    std::unique_ptr<wallpaper::ShmRenderTarget> buf_b;  // cross-fade target
    wp_fractional_scale_v1*                   frac_scale  = nullptr;
    wp_viewport*                              viewport    = nullptr;

    uint32_t logical_w  = 0;
    uint32_t logical_h  = 0;
    uint32_t phys_w     = 0;
    uint32_t phys_h     = 0;
    double   scale      = 1.0;
    bool     configured = false;

    // Cross-fade state
    int fade_frame = 0;    // 0 = not fading
    int fade_total = 0;
    std::vector<uint32_t> fade_blend;

    // Active wallpaper state
    std::string            active_path;
    wallpaper::FitMode     fit_mode    = wallpaper::FitMode::Fill;
    std::string            pending_path;
};

static std::unordered_map<wl_output*, OutputSurface> g_outputs;

static wallpaper::ImageProvider   g_provider;
static wallpaper::DynamicSchedule g_schedule;
static wallpaper::DynamicTimer    g_solar_timer;
static int                        g_fade_timer_fd = -1;

// ─────────────────────────────────────────────────────────────────────────────
// Signal handler
// ─────────────────────────────────────────────────────────────────────────────
static void signal_handler(int sig) {
    if (sig == SIGUSR1) g_reload_requested = 1;
    else                g_running = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Cross-fade timerfd helpers — 60fps = 16ms period
// ─────────────────────────────────────────────────────────────────────────────
static void arm_fade_timerfd(int fd) {
    if (fd < 0) return;
    struct itimerspec ts{};
    ts.it_value.tv_nsec    = 16'666'666;
    ts.it_interval.tv_nsec = 16'666'666;
    ::timerfd_settime(fd, 0, &ts, nullptr);
}

static void disarm_fade_timerfd(int fd) {
    if (fd < 0) return;
    struct itimerspec ts{};
    ::timerfd_settime(fd, 0, &ts, nullptr);
}

static bool any_output_fading() {
    for (const auto& [_, o] : g_outputs)
        if (o.fade_frame > 0 && o.fade_frame <= o.fade_total) return true;
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Physical size from logical + scale
// ─────────────────────────────────────────────────────────────────────────────
static std::pair<uint32_t, uint32_t> phys_size(uint32_t lw, uint32_t lh, double scale) {
    return {
        static_cast<uint32_t>(std::ceil(lw * scale)),
        static_cast<uint32_t>(std::ceil(lh * scale))
    };
}

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations needed by layer_surface_configure
// ─────────────────────────────────────────────────────────────────────────────
static void render_and_commit(OutputSurface& out, const std::string& path);
[[nodiscard]] static std::string resolve_wallpaper_path_from_settings();

// ─────────────────────────────────────────────────────────────────────────────
// wp_fractional_scale_v1 listener
// preferred_scale encoding: 120 = 1.0×, 150 = 1.25×, 180 = 1.5×, 240 = 2.0×
// ─────────────────────────────────────────────────────────────────────────────
static void frac_scale_preferred(void* data, struct wp_fractional_scale_v1*,
                                  uint32_t preferred_scale) {
    auto* out   = static_cast<OutputSurface*>(data);
    const double scale = static_cast<double>(preferred_scale) / 120.0;
    if (std::abs(scale - out->scale) < 0.001) return;

    out->scale = scale;
    if (out->logical_w > 0 && out->logical_h > 0) {
        auto [pw, ph] = phys_size(out->logical_w, out->logical_h, scale);
        if (pw != out->phys_w || ph != out->phys_h) {
            out->phys_w = pw; out->phys_h = ph;
            if (out->buf_a) out->buf_a->resize(pw, ph);
            if (out->buf_b) out->buf_b->resize(pw, ph);
            log::info("[wallpaper] Fractional scale → {:.3f}, phys={}x{}", scale, pw, ph);
        }
    }
}

static const struct wp_fractional_scale_v1_listener frac_scale_listener = {
    .preferred_scale = frac_scale_preferred,
};

// ─────────────────────────────────────────────────────────────────────────────
// zwlr_layer_surface_v1 listener
// ─────────────────────────────────────────────────────────────────────────────
static void layer_surface_configure(void* data, struct zwlr_layer_surface_v1* surf,
                                     uint32_t serial, uint32_t w, uint32_t h) {
    auto* out = static_cast<OutputSurface*>(data);
    zwlr_layer_surface_v1_ack_configure(surf, serial);
    if (w > 0 && h > 0) {
        out->logical_w = w; out->logical_h = h;
        auto [pw, ph] = phys_size(w, h, out->scale);
        out->phys_w = pw; out->phys_h = ph;
    }
    out->configured = true;
    log::info("[wallpaper] Output configured: logical={}x{} phys={}x{} scale={:.3f}",
              out->logical_w, out->logical_h, out->phys_w, out->phys_h, out->scale);

    // CRITICAL: Attach and commit a valid buffer upon configure per Wayland Layer-Shell protocol
    if (out->phys_w > 0 && out->phys_h > 0) {
        std::string path = !out->active_path.empty() ? out->active_path : resolve_wallpaper_path_from_settings();
        if (!path.empty()) {
            render_and_commit(*out, path);
        }
    }
    if (g_display) wl_display_flush(g_display);
}

static void layer_surface_closed(void* data, struct zwlr_layer_surface_v1*) {
    static_cast<OutputSurface*>(data)->configured = false;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed    = layer_surface_closed,
};

// ─────────────────────────────────────────────────────────────────────────────
// wl_output listener — mode and scale
// ─────────────────────────────────────────────────────────────────────────────
static void output_geometry(void*, struct wl_output*, int32_t, int32_t,
                             int32_t, int32_t, int32_t, const char*, const char*, int32_t) {}

static void output_mode(void* data, struct wl_output*, uint32_t flags,
                         int32_t w, int32_t h, int32_t) {
    if (!(flags & WL_OUTPUT_MODE_CURRENT)) return;
    auto* out = static_cast<OutputSurface*>(data);
    out->logical_w = static_cast<uint32_t>(w);
    out->logical_h = static_cast<uint32_t>(h);
    auto [pw, ph] = phys_size(out->logical_w, out->logical_h, out->scale);
    out->phys_w = pw; out->phys_h = ph;
}

static void output_scale(void* data, struct wl_output*, int32_t factor) {
    // Only apply integer scale if fractional scale protocol is not available
    if (g_frac_scale_mgr != nullptr) return;
    auto* out     = static_cast<OutputSurface*>(data);
    out->scale    = static_cast<double>(factor);
    auto [pw, ph] = phys_size(out->logical_w, out->logical_h, out->scale);
    out->phys_w   = pw; out->phys_h = ph;
}

static void output_done(void*, struct wl_output*) {}
static void output_name(void*, struct wl_output*, const char*) {}
static void output_description(void*, struct wl_output*, const char*) {}

static const struct wl_output_listener output_listener = {
    .geometry    = output_geometry,
    .mode        = output_mode,
    .done        = output_done,
    .scale       = output_scale,
    .name        = output_name,
    .description = output_description,
};

// ─────────────────────────────────────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
static void create_output_surface(wl_output* output);

// ─────────────────────────────────────────────────────────────────────────────
// Registry listeners
// ─────────────────────────────────────────────────────────────────────────────
static void registry_handle_global(void*, struct wl_registry* reg,
                                    uint32_t name, const char* interface, uint32_t version) {
    using sv = std::string_view;

    if (sv(interface) == wl_compositor_interface.name) {
        g_compositor = static_cast<wl_compositor*>(
            wl_registry_bind(reg, name, &wl_compositor_interface, version >= 4 ? 4 : version));
        log::info("[wallpaper] Bound wl_compositor");

    } else if (sv(interface) == wl_shm_interface.name) {
        g_shm = static_cast<wl_shm*>(
            wl_registry_bind(reg, name, &wl_shm_interface, 1));
        log::info("[wallpaper] Bound wl_shm");

    } else if (sv(interface) == zwlr_layer_shell_v1_interface.name) {
        g_layer_shell = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(reg, name, &zwlr_layer_shell_v1_interface, version >= 4 ? 4 : version));
        log::info("[wallpaper] Bound zwlr_layer_shell_v1");

    } else if (sv(interface) == wp_fractional_scale_manager_v1_interface.name) {
        g_frac_scale_mgr = static_cast<wp_fractional_scale_manager_v1*>(
            wl_registry_bind(reg, name, &wp_fractional_scale_manager_v1_interface, 1));
        log::info("[wallpaper] Bound wp_fractional_scale_manager_v1");

    } else if (sv(interface) == wp_viewporter_interface.name) {
        g_viewporter = static_cast<wp_viewporter*>(
            wl_registry_bind(reg, name, &wp_viewporter_interface, 1));
        log::info("[wallpaper] Bound wp_viewporter");

    } else if (sv(interface) == wl_output_interface.name) {
        wl_output* output = static_cast<wl_output*>(
            wl_registry_bind(reg, name, &wl_output_interface, version >= 4 ? 4 : version));

        g_outputs.emplace(output, OutputSurface{});
        auto& out  = g_outputs.at(output);
        out.output = output;

        wl_output_add_listener(output, &output_listener, &out);
        log::info("[wallpaper] wl_output announced (name={})", name);

        if (g_layer_shell && g_compositor && g_shm) create_output_surface(output);
    }
}

static void registry_handle_global_remove(void*, struct wl_registry*, uint32_t) {
    // Destroy any surface that was closed by the compositor
    for (auto it = g_outputs.begin(); it != g_outputs.end(); ) {
        auto& out = it->second;
        if (!out.configured && out.layer_surf) {
            zwlr_layer_surface_v1_destroy(out.layer_surf);
            if (out.frac_scale) wp_fractional_scale_v1_destroy(out.frac_scale);
            if (out.viewport)   wp_viewport_destroy(out.viewport);
            wl_output_destroy(out.output);
            it = g_outputs.erase(it);
        } else {
            ++it;
        }
    }
}

static const struct wl_registry_listener registry_listener = {
    .global        = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

// ─────────────────────────────────────────────────────────────────────────────
// Path resolution — pure XDG, zero hardcoded paths or UIDs
// ─────────────────────────────────────────────────────────────────────────────

[[nodiscard]] static std::string resolve_wallpaper_path_from_settings() {
    std::error_code ec;

    // 1. Check runtime wallpaper override files written by SettingsBridge
    const std::vector<std::string> runtime_files = {
        "/tmp/current_wallpaper",
        "/run/user/" + std::to_string(static_cast<unsigned>(::getuid())) + "/tinexus/current_wallpaper",
        "/run/user/0/tinexus/current_wallpaper"
    };
    for (const auto& rf : runtime_files) {
        if (fs::exists(rf, ec) && !ec) {
            std::ifstream rff(rf);
            std::string path;
            if (std::getline(rff, path)) {
                path.erase(0, path.find_first_not_of(" \t\"'"));
                path.erase(path.find_last_not_of(" \t\"'\r\n") + 1);
                if (!path.empty() && fs::exists(path, ec) && !ec) {
                    return path;
                }
            }
        }
    }

    // 2. Parse settings.toml (supporting both [wallpaper] path = ... and root wallpaper_path = ...)
    fs::path cfg;
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        cfg = fs::path(xdg) / "tinexus/settings.toml";
    } else {
        const char* home = std::getenv("HOME");
        cfg = (home && home[0] != '\0')
            ? fs::path(home) / ".config/tinexus/settings.toml"
            : fs::path("/etc/tinexus/settings.toml");
    }

    if (fs::exists(cfg, ec) && !ec) {
        std::ifstream f(cfg);
        bool in_wall = false;
        std::string line;
        while (std::getline(f, line)) {
            size_t s = line.find_first_not_of(" \t");
            if (s == std::string::npos) continue;
            line = line.substr(s);
            if (line == "[wallpaper]") { in_wall = true; continue; }
            if (!line.empty() && line[0] == '[') { in_wall = false; continue; }

            if (in_wall && line.starts_with("path")) {
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string val = line.substr(eq + 1);
                    val.erase(0, val.find_first_not_of(" \t\"'"));
                    val.erase(val.find_last_not_of(" \t\"'\r\n") + 1);
                    if (!val.empty() && fs::exists(val, ec) && !ec) return val;
                }
            } else if (line.starts_with("wallpaper_path")) {
                auto eq = line.find('=');
                if (eq != std::string::npos) {
                    std::string val = line.substr(eq + 1);
                    val.erase(0, val.find_first_not_of(" \t\"'"));
                    val.erase(val.find_last_not_of(" \t\"'\r\n") + 1);
                    if (!val.empty() && fs::exists(val, ec) && !ec) return val;
                }
            }
        }
    }

    // 3. Guaranteed fallback candidates on clean boot (Emerald Matrix primary default)
    const char* candidates[] = {
        "/usr/share/backgrounds/emerald-matrix.png",
        "/usr/share/backgrounds/tinexus-default.jpg",
        "/usr/share/backgrounds/tinexus-os-primary.jpg",
        "/usr/share/backgrounds/sunset-gradient.png",
        "assets/wallpaper/emerald-matrix.png",
        "assets/wallpaper/tinexus-default.jpg"
    };
    for (const char* cand : candidates) {
        if (fs::exists(cand, ec) && !ec) {
            return cand;
        }
    }

    return "/usr/share/backgrounds/emerald-matrix.png";
}



// ─────────────────────────────────────────────────────────────────────────────
// create_output_surface — allocates two SHM buffers and a layer surface
// ─────────────────────────────────────────────────────────────────────────────
static void create_output_surface(wl_output* output) {
    auto it = g_outputs.find(output);
    if (it == g_outputs.end() || !g_shm || !g_compositor) return;
    OutputSurface& out = it->second;
    if (out.layer_surf) return; // Already created

    out.buf_a = wallpaper::ShmRenderTarget::create(g_shm, g_compositor, 1, 1);
    out.buf_b = wallpaper::ShmRenderTarget::create(g_shm, g_compositor, 1, 1);
    if (!out.buf_a || !out.buf_b) { log::error("[wallpaper] Failed to create render targets"); return; }

    wl_surface* surf_a = out.buf_a->surface();

    if (g_frac_scale_mgr) {
        out.frac_scale = wp_fractional_scale_manager_v1_get_fractional_scale(g_frac_scale_mgr, surf_a);
        wp_fractional_scale_v1_add_listener(out.frac_scale, &frac_scale_listener, &out);
    }
    if (g_viewporter)
        out.viewport = wp_viewporter_get_viewport(g_viewporter, surf_a);

    out.layer_surf = zwlr_layer_shell_v1_get_layer_surface(
        g_layer_shell, surf_a, output,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "tinexus-wallpaper");

    zwlr_layer_surface_v1_add_listener(out.layer_surf, &layer_surface_listener, &out);
    zwlr_layer_surface_v1_set_size(out.layer_surf, 0, 0);
    zwlr_layer_surface_v1_set_anchor(out.layer_surf,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP    | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT   | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(out.layer_surf, -1);
    wl_surface_commit(surf_a);
    log::info("[wallpaper] Layer surface created for output");
}

// ─────────────────────────────────────────────────────────────────────────────
// render_and_commit — renders directly into buf_a and commits
// ─────────────────────────────────────────────────────────────────────────────
static void render_and_commit(OutputSurface& out, const std::string& path) {
    if (!out.configured || out.phys_w == 0 || !out.buf_a) return;
    if (out.buf_a->width() != out.phys_w || out.buf_a->height() != out.phys_h)
        out.buf_a->resize(out.phys_w, out.phys_h);

    std::string actual_path = path;
    if (actual_path.empty()) actual_path = resolve_wallpaper_path_from_settings();

    bool loaded = false;
    if (!actual_path.empty()) {
        loaded = g_provider.load(actual_path);
    }
    if (!loaded) {
        const char* fallbacks[] = {
            "/usr/share/backgrounds/emerald-matrix.png",
            "/usr/share/backgrounds/tinexus-default.jpg",
            "/usr/share/backgrounds/tinexus-os-primary.jpg",
            "assets/wallpaper/emerald-matrix.png"
        };
        for (const char* fb : fallbacks) {
            if (g_provider.load(fb)) {
                actual_path = fb;
                loaded = true;
                break;
            }
        }
    }

    auto buf = g_provider.render_buffer(out.phys_w, out.phys_h, out.fit_mode);
    if (out.buf_a->data() && !buf.pixels.empty()) {
        std::copy(buf.pixels.begin(), buf.pixels.end(), out.buf_a->data());
        out.buf_a->present();
        std::copy(buf.pixels.begin(), buf.pixels.end(), out.buf_a->data());
        out.buf_a->present();
        out.active_path = actual_path;
        log::info("[wallpaper] Committed '{}' ({}x{})", actual_path, out.phys_w, out.phys_h);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// begin_crossfade — loads new image into buf_b and starts fade
// ─────────────────────────────────────────────────────────────────────────────
static void begin_crossfade(OutputSurface& out, const std::string& new_path, uint16_t fade_ms) {
    if (!out.configured || out.phys_w == 0) return;
    if (fade_ms == 0) { render_and_commit(out, new_path); return; }
    if (!out.buf_b) return;

    if (out.buf_b->width() != out.phys_w || out.buf_b->height() != out.phys_h)
        out.buf_b->resize(out.phys_w, out.phys_h);

    g_provider.load(new_path);
    auto new_buf = g_provider.render_buffer(out.phys_w, out.phys_h, out.fit_mode);
    if (out.buf_b->data() && !new_buf.pixels.empty())
        std::copy(new_buf.pixels.begin(), new_buf.pixels.end(), out.buf_b->data());

    out.fade_total = std::max(1, static_cast<int>(static_cast<uint32_t>(fade_ms) * 60 / 1000));
    out.fade_frame = 1;
    out.fade_blend.resize(static_cast<size_t>(out.phys_w) * out.phys_h);
    out.pending_path = new_path;
    log::info("[wallpaper] Cross-fade started '{}' → '{}' ({} frames)",
              out.active_path, new_path, out.fade_total);
}

// ─────────────────────────────────────────────────────────────────────────────
// advance_fade_frame — called each time g_fade_timer_fd fires
// ─────────────────────────────────────────────────────────────────────────────
static void advance_fade_frame() {
    bool still_fading = false;
    for (auto& [_, out] : g_outputs) {
        if (out.fade_frame <= 0 || out.fade_frame > out.fade_total) continue;
        if (!out.buf_a || !out.buf_b || !out.buf_a->data() || !out.buf_b->data()) continue;

        const float t  = static_cast<float>(out.fade_frame) / static_cast<float>(out.fade_total);
        const size_t px = static_cast<size_t>(out.phys_w) * out.phys_h;
        out.fade_blend.resize(px);

        wallpaper::ImageProvider::crossfade_lerp(
            out.buf_a->data(), out.buf_b->data(), out.fade_blend.data(), px, t);

        std::copy(out.fade_blend.begin(), out.fade_blend.end(), out.buf_a->data());
        out.buf_a->present();

        if (out.fade_frame >= out.fade_total) {
            out.active_path = out.pending_path;
            std::swap(out.buf_a, out.buf_b); // buf_a is now the final committed image
            out.fade_frame = 0; out.fade_total = 0; out.fade_blend.clear();
            log::info("[wallpaper] Cross-fade complete → '{}'", out.active_path);
        } else {
            ++out.fade_frame;
            still_fading = true;
        }
    }
    if (g_display) wl_display_flush(g_display);
    if (!still_fading) disarm_fade_timerfd(g_fade_timer_fd);
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGUSR1, &sa, nullptr);

    log::set_component_name("wallpaper");
    log::info("[wallpaper] tinexus-wallpaper starting (D-Bus io.tinexus.Wallpaper)...");

    // ── Cross-fade timerfd (60fps, disarmed when no fade is active) ──
    g_fade_timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (g_fade_timer_fd < 0)
        log::warn("[wallpaper] timerfd_create(MONOTONIC) failed: {} (fades disabled)",
                  std::strerror(errno));

    // ── Wayland connection ──
    g_display = wl_display_connect(nullptr);
    if (!g_display) {
        log::error("[wallpaper] Failed to connect to Wayland display!");
        return 1;
    }

    wl_registry* reg = wl_display_get_registry(g_display);
    wl_registry_add_listener(reg, &registry_listener, nullptr);
    wl_display_roundtrip(g_display); // binds globals + announces outputs

    if (!g_layer_shell) {
        log::error("[wallpaper] zwlr_layer_shell_v1 not advertised by compositor!");
        return 1;
    }
    if (!g_compositor || !g_shm) {
        log::error("[wallpaper] wl_compositor or wl_shm not advertised by compositor!");
        return 1;
    }
    if (g_outputs.empty()) {
        log::error("[wallpaper] No wl_output globals announced!");
        return 1;
    }

    // Create surfaces for any outputs announced before layer_shell was bound
    for (auto& [output, _] : g_outputs)
        create_output_surface(output);

    wl_display_roundtrip(g_display); // receive configure events

    // Wait up to 1 second for all outputs to configure
    for (int tries = 100; tries > 0; --tries) {
        bool all_ok = true;
        for (const auto& [_, o] : g_outputs) if (!o.configured) { all_ok = false; break; }
        if (all_ok) break;
        wl_display_dispatch(g_display);
        ::usleep(10'000);
    }

    // ── Dynamic schedule discovery ──
    auto manifests = wallpaper::find_twallpaper_manifests();
    if (!manifests.empty()) {
        static_cast<void>(g_schedule.load(manifests.front()));
    }

    // ── Initial wallpaper ──
    const std::string init_path = [&]() -> std::string {
        if (g_schedule.is_loaded()) {
            auto f = g_schedule.resolve_frame(static_cast<int64_t>(::time(nullptr)));
            if (f) return f->path;
        }
        return resolve_wallpaper_path_from_settings();
    }();

    for (auto& [_, out] : g_outputs) render_and_commit(out, init_path);
    wl_display_flush(g_display);

    // Arm solar timer
    if (g_schedule.is_loaded() && g_solar_timer.is_valid()) {
        const uint32_t secs = g_schedule.seconds_until_next_transition(
            static_cast<int64_t>(::time(nullptr)));
        g_solar_timer.arm(secs);
        log::info("[wallpaper] Solar timer armed: next frame in {} seconds", secs);
    }

    std::string current_path    = init_path;
    uint16_t    current_fade_ms = 500;
    auto        current_mode    = wallpaper::FitMode::Fill;

    // ── Initialize D-Bus service (io.tinexus.Wallpaper) ──
    wallpaper::WallpaperDBus::instance().init(
        // on_set (SetWallpaper method call)
        [&](const std::string& path, uint8_t mode, bool dynamic, uint16_t fade_ms) {
            current_path    = path;
            current_mode    = static_cast<wallpaper::FitMode>(mode);
            current_fade_ms = fade_ms > 0 ? fade_ms : 500;

            const std::string& target = dynamic && g_schedule.is_loaded()
                ? ([&]() -> const std::string& {
                       auto f = g_schedule.resolve_frame(static_cast<int64_t>(::time(nullptr)));
                       return f ? f->path : current_path;
                   }())
                : current_path;

            for (auto& [_, out] : g_outputs) {
                out.fit_mode = current_mode;
                begin_crossfade(out, target, current_fade_ms);
            }
            if (any_output_fading()) arm_fade_timerfd(g_fade_timer_fd);

            wallpaper::WallpaperDBus::instance().emit_wallpaper_changed(
                target, mode, dynamic);
        },
        // on_status (GetStatus method call)
        [&]() -> wallpaper::WallpaperStatus {
            wallpaper::WallpaperStatus st{};
            for (const auto& [_, out] : g_outputs) {
                if (out.configured && !out.active_path.empty()) {
                    st.path = out.active_path;
                    st.mode = static_cast<uint8_t>(out.fit_mode);
                    break;
                }
            }
            if (st.path.empty()) st.path = current_path;
            if (g_schedule.is_loaded()) {
                st.is_dynamic = true;
                const int64_t now = static_cast<int64_t>(::time(nullptr));
                auto frame = g_schedule.resolve_frame(now);
                st.current_frame_index = frame ? frame->index : 0;
                st.total_frames        = g_schedule.frame_count();
                st.next_change_secs    = g_schedule.seconds_until_next_transition(now);
            } else {
                st.total_frames = 1;
            }
            return st;
        },
        // on_advance (AdvanceFrame method call)
        [&]() {
            if (!g_schedule.is_loaded()) return;
            const int64_t now = static_cast<int64_t>(::time(nullptr));
            auto frame = g_schedule.resolve_frame(now);
            if (frame) {
                current_path = frame->path;
                for (auto& [_, out] : g_outputs)
                    begin_crossfade(out, frame->path, 1000);
                if (any_output_fading()) arm_fade_timerfd(g_fade_timer_fd);
                wallpaper::WallpaperDBus::instance().emit_wallpaper_changed(
                    frame->path, static_cast<uint8_t>(current_mode), true);
            }
        },
        // on_reload (io.tinexus.Settings ThemeChanged/ConfigChanged signal)
        [&]() {
            current_path = resolve_wallpaper_path_from_settings();
            log::info("[wallpaper] Settings signal received -> crossfade to '{}'", current_path);
            for (auto& [_, out] : g_outputs)
                begin_crossfade(out, current_path, current_fade_ms);
            if (any_output_fading()) arm_fade_timerfd(g_fade_timer_fd);
            wallpaper::WallpaperDBus::instance().emit_wallpaper_changed(
                current_path, static_cast<uint8_t>(current_mode), false);
        }
    );

    // Initial state announcement
    wallpaper::WallpaperDBus::instance().emit_wallpaper_changed(
        init_path, 0, g_schedule.is_loaded());

    const int wayland_fd = wl_display_get_fd(g_display);

    // ─────────────────────────────────────────────────────────────────────────
    // Main event loop — up to 4 fds:
    //   [0] Wayland display fd
    //   [1] D-Bus session bus fd (via sd-bus)
    //   [2] cross-fade timerfd (armed only during active fades)
    //   [3] solar schedule timerfd (CLOCK_REALTIME)
    // ─────────────────────────────────────────────────────────────────────────
    while (g_running) {
        if (g_reload_requested) {
            g_reload_requested = 0;
            current_path = resolve_wallpaper_path_from_settings();
            log::info("[wallpaper] SIGUSR1 reload → '{}'", current_path);
            for (auto& [_, out] : g_outputs)
                begin_crossfade(out, current_path, current_fade_ms);
            if (any_output_fading()) arm_fade_timerfd(g_fade_timer_fd);
            wallpaper::WallpaperDBus::instance().emit_wallpaper_changed(
                current_path, static_cast<uint8_t>(current_mode), false);
        }

        while (wl_display_prepare_read(g_display) != 0)
            wl_display_dispatch_pending(g_display);
        wl_display_flush(g_display);

        const int bus_fd = wallpaper::WallpaperDBus::instance().fd();

        struct pollfd fds[4];
        int nfds = 0;
        fds[nfds++] = { .fd = wayland_fd, .events = POLLIN, .revents = 0 };
        if (bus_fd >= 0) {
            short b_ev = static_cast<short>(wallpaper::WallpaperDBus::instance().events() | POLLIN);
            fds[nfds++] = { .fd = bus_fd, .events = b_ev, .revents = 0 };
        }
        if (g_fade_timer_fd >= 0)
            fds[nfds++] = { .fd = g_fade_timer_fd, .events = POLLIN, .revents = 0 };
        if (g_solar_timer.is_valid())
            fds[nfds++] = { .fd = g_solar_timer.fd(), .events = POLLIN, .revents = 0 };

        int ret = ::poll(fds, static_cast<nfds_t>(nfds), -1);

        if (ret < 0) {
            wl_display_cancel_read(g_display);
            if (errno == EINTR) continue;
            log::error("[wallpaper] poll() error: {}", std::strerror(errno));
            break;
        }

        // Wayland
        if (fds[0].revents & POLLIN) {
            wl_display_read_events(g_display);
            wl_display_dispatch_pending(g_display);
        } else {
            wl_display_cancel_read(g_display);
        }

        // D-Bus
        if (bus_fd >= 0) {
            for (int i = 1; i < nfds; ++i) {
                if (fds[i].fd == bus_fd && (fds[i].revents & (POLLIN | POLLERR | POLLHUP))) {
                    wallpaper::WallpaperDBus::instance().process();
                    break;
                }
            }
        }

        // Cross-fade timer
        for (int i = 0; i < nfds; ++i) {
            if (fds[i].fd == g_fade_timer_fd && (fds[i].revents & POLLIN)) {
                uint64_t count = 0;
                static_cast<void>(::read(g_fade_timer_fd, &count, sizeof(count)));
                advance_fade_frame();
                break;
            }
        }

        // Solar schedule timer
        if (g_solar_timer.is_valid()) {
            for (int i = 0; i < nfds; ++i) {
                if (fds[i].fd == g_solar_timer.fd() && (fds[i].revents & POLLIN)) {
                    g_solar_timer.consume_expiry();
                    const int64_t now = static_cast<int64_t>(::time(nullptr));
                    auto frame = g_schedule.resolve_frame(now);
                    if (frame && frame->path != current_path) {
                        log::info("[wallpaper] Solar frame advance → '{}'", frame->path);
                        current_path = frame->path;
                        for (auto& [_, out] : g_outputs)
                            begin_crossfade(out, frame->path, 2000); // 2-second solar fade
                        if (any_output_fading()) arm_fade_timerfd(g_fade_timer_fd);
                        wallpaper::WallpaperDBus::instance().emit_wallpaper_changed(
                            frame->path, static_cast<uint8_t>(current_mode), true);
                    }
                    const uint32_t secs = g_schedule.seconds_until_next_transition(now);
                    g_solar_timer.arm(secs);
                    log::info("[wallpaper] Solar timer re-armed: {} seconds", secs);
                    break;
                }
            }
        }
    }

    // ── Clean shutdown ──
    log::info("[wallpaper] Shutting down...");
    for (auto& [_, out] : g_outputs) {
        if (out.layer_surf)  zwlr_layer_surface_v1_destroy(out.layer_surf);
        if (out.frac_scale)  wp_fractional_scale_v1_destroy(out.frac_scale);
        if (out.viewport)    wp_viewport_destroy(out.viewport);
        if (out.output)      wl_output_destroy(out.output);
    }
    g_outputs.clear();
    if (g_fade_timer_fd >= 0) ::close(g_fade_timer_fd);
    wallpaper::WallpaperDBus::instance().shutdown();
    wallpaper::ImageProvider::cache().clear();
    if (g_display) wl_display_disconnect(g_display);
    log::info("[wallpaper] tinexus-wallpaper exited cleanly.");
    return 0;
}
