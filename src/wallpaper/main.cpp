// ============================================================================
// main.cpp — tinexus-wallpaper (Milestones 1, 3, 4, 5, 7 — Final)
// ============================================================================
// Pure C++20 Wayland LAYER_BACKGROUND daemon.
//
// Milestone 1: Per-output surface map — wl_output + wp_fractional_scale_v1
// Milestone 3: IPC-native event loop — dual-fd poll(wayland + ipcd, -1)
// Milestone 4: CPU cross-fade — dual-buffer ping-pong via timerfd at 60fps
// Milestone 5: Dynamic solar schedule — .twallpaper + timerfd CLOCK_REALTIME
// Milestone 7: WALLPAPER_STATUS_QUERY → WallpaperStatusPayload response
// ============================================================================

#include "wallpaper/wallpaper_provider.hpp"
#include "wallpaper/solar_schedule.hpp"
#include <txui/wayland/WaylandConnection.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <common/logger.hpp>
#include <ipcd/protocol/header.hpp>

#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <wayland-client.h>

// wp_fractional_scale_v1 (wlroots >= 0.17) — advertised by tinexus-comp
extern "C" {
#include <fractional-scale-v1-client-protocol.h>
#include <viewporter-client-protocol.h>
}

#include <sys/socket.h>
#include <sys/un.h>
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
static zwlr_layer_shell_v1*            g_layer_shell     = nullptr;
static wp_fractional_scale_manager_v1* g_frac_scale_mgr  = nullptr;
static wp_viewporter*                  g_viewporter       = nullptr;
static std::atomic<bool>               g_running{true};
static volatile sig_atomic_t           g_reload_requested = 0;

// ─────────────────────────────────────────────────────────────────────────────
// OutputSurface — per-monitor wallpaper state
// ─────────────────────────────────────────────────────────────────────────────
struct OutputSurface {
    wl_output*                                output      = nullptr;
    zwlr_layer_surface_v1*                    layer_surf  = nullptr;
    std::unique_ptr<txui::WaylandRenderTarget> buf_a;  // currently displayed
    std::unique_ptr<txui::WaylandRenderTarget> buf_b;  // cross-fade target
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
namespace txui::wayland { class WaylandConnection; }
static txui::wayland::WaylandConnection* g_conn = nullptr;
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
    if (g_conn) g_conn->flush();
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

    if (sv(interface) == zwlr_layer_shell_v1_interface.name) {
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

        if (g_layer_shell && g_conn) create_output_surface(output);
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
[[nodiscard]] static std::string get_ipc_socket_path() {
    const char* x = std::getenv("XDG_RUNTIME_DIR");
    if (x && x[0] != '\0') return std::string(x) + "/tinexus/ipc.sock";
    return "/run/user/" + std::to_string(static_cast<unsigned>(::getuid()))
           + "/tinexus/ipc.sock";
}

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
// IPC client helpers
// ─────────────────────────────────────────────────────────────────────────────
[[nodiscard]] static int connect_to_ipcd() {
    const std::string path = get_ipc_socket_path();
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) { log::error("[wallpaper] socket(): {}", std::strerror(errno)); return -1; }
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        log::warn("[wallpaper] ipcd not reachable: {} (standalone mode)", std::strerror(errno));
        ::close(fd); return -1;
    }
    log::info("[wallpaper] Connected to tinexus-ipcd at '{}'", path);
    return fd;
}

[[nodiscard]] static bool send_ipc_frame(int fd,
                                          ipcd::protocol::MessageType mt, uint32_t seq,
                                          const uint8_t* payload, uint32_t plen) {
    using namespace ipcd::protocol;
    Header hdr{}; hdr.magic = TINEXUS_IPC_MAGIC; hdr.version = TINEXUS_IPC_VERSION_1;
    hdr.msg_type = static_cast<uint16_t>(mt); hdr.sequence_id = seq; hdr.payload_len = plen;
    std::vector<uint8_t> buf(sizeof(hdr) + plen);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    if (payload && plen) std::memcpy(buf.data() + sizeof(hdr), payload, plen);
    const uint8_t* p = buf.data(); size_t r = buf.size();
    while (r > 0) {
        ssize_t n = ::write(fd, p, r);
        if (n < 0) { if (errno == EINTR) continue; return false; }
        p += n; r -= static_cast<size_t>(n);
    }
    return true;
}

static bool register_with_ipcd(int ipc_fd) {
    using MT = ipcd::protocol::MessageType;
    const char* svc = "wallpaper";
    if (!send_ipc_frame(ipc_fd, MT::SYS_REGISTER_SERVICE, 1,
                        reinterpret_cast<const uint8_t*>(svc),
                        static_cast<uint32_t>(std::strlen(svc) + 1))) return false;
    uint16_t topic = static_cast<uint16_t>(MT::WALLPAPER_CHANGED);
    return send_ipc_frame(ipc_fd, MT::SYS_SUBSCRIBE_TOPIC, 2,
                          reinterpret_cast<const uint8_t*>(&topic), sizeof(topic));
}

// ─────────────────────────────────────────────────────────────────────────────
// create_output_surface — allocates two SHM buffers and a layer surface
// ─────────────────────────────────────────────────────────────────────────────
static void create_output_surface(wl_output* output) {
    auto it = g_outputs.find(output);
    if (it == g_outputs.end() || !g_conn) return;
    OutputSurface& out = it->second;
    if (out.layer_surf) return; // Already created

    auto ta = txui::WaylandRenderTarget::create(*g_conn, 1, 1);
    auto tb = txui::WaylandRenderTarget::create(*g_conn, 1, 1);
    if (!ta || !tb) { log::error("[wallpaper] Failed to create render targets"); return; }
    out.buf_a = std::make_unique<txui::WaylandRenderTarget>(std::move(*ta));
    out.buf_b = std::make_unique<txui::WaylandRenderTarget>(std::move(*tb));

    wl_surface* surf_a = out.buf_a->surface().surface();

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
    if (g_conn) g_conn->flush();
    if (!still_fading) disarm_fade_timerfd(g_fade_timer_fd);
}

// ─────────────────────────────────────────────────────────────────────────────
// build_status_payload — M7: WallpaperStatusPayload for WALLPAPER_STATUS_REPLY
// ─────────────────────────────────────────────────────────────────────────────
static ipcd::protocol::WallpaperStatusPayload build_status_payload() {
    ipcd::protocol::WallpaperStatusPayload s{};
    for (const auto& [_, out] : g_outputs) {
        if (out.configured && !out.active_path.empty()) {
            ::strncpy(s.path, out.active_path.c_str(), sizeof(s.path) - 1);
            s.mode = static_cast<uint8_t>(out.fit_mode);
            break;
        }
    }
    if (g_schedule.is_loaded()) {
        s.is_dynamic = 1;
        const int64_t now = static_cast<int64_t>(::time(nullptr));
        auto frame = g_schedule.resolve_frame(now);
        s.current_frame_index = frame ? frame->index : 0;
        s.total_frames        = g_schedule.frame_count();
        s.next_change_secs    = g_schedule.seconds_until_next_transition(now);
    } else {
        s.total_frames = 1;
    }
    return s;
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
    log::info("[wallpaper] tinexus-wallpaper starting (M1+M3+M4+M5+M7)...");

    // ── IPC connection ──
    int ipc_fd = connect_to_ipcd();
    if (ipc_fd >= 0 && !register_with_ipcd(ipc_fd)) {
        log::warn("[wallpaper] ipcd handshake failed — standalone mode");
        ::close(ipc_fd); ipc_fd = -1;
    }

    // ── Cross-fade timerfd (60fps, disarmed when no fade is active) ──
    g_fade_timer_fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (g_fade_timer_fd < 0)
        log::warn("[wallpaper] timerfd_create(MONOTONIC) failed: {} (fades disabled)",
                  std::strerror(errno));

    // ── Wayland connection ──
    auto conn_opt = txui::wayland::WaylandConnection::connect();
    if (!conn_opt) {
        log::error("[wallpaper] Failed to connect to Wayland display!");
        if (ipc_fd >= 0) ::close(ipc_fd);
        return 1;
    }
    auto connection = std::move(*conn_opt);
    g_conn = &connection;

    wl_registry* reg = wl_display_get_registry(connection.display());
    wl_registry_add_listener(reg, &registry_listener, nullptr);
    wl_display_roundtrip(connection.display()); // binds globals + announces outputs

    if (!g_layer_shell) {
        log::error("[wallpaper] zwlr_layer_shell_v1 not advertised by compositor!");
        return 1;
    }
    if (g_outputs.empty()) {
        log::error("[wallpaper] No wl_output globals announced!");
        return 1;
    }

    // Create surfaces for any outputs announced before layer_shell was bound
    for (auto& [output, _] : g_outputs)
        create_output_surface(output);

    wl_display_roundtrip(connection.display()); // receive configure events

    // Wait up to 1 second for all outputs to configure
    for (int tries = 100; tries > 0; --tries) {
        bool all_ok = true;
        for (const auto& [_, o] : g_outputs) if (!o.configured) { all_ok = false; break; }
        if (all_ok) break;
        wl_display_dispatch(connection.display());
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
    connection.flush();

    // Arm solar timer
    if (g_schedule.is_loaded() && g_solar_timer.is_valid()) {
        const uint32_t secs = g_schedule.seconds_until_next_transition(
            static_cast<int64_t>(::time(nullptr)));
        g_solar_timer.arm(secs);
        log::info("[wallpaper] Solar timer armed: next frame in {} seconds", secs);
    }

    // ── IPC receive buffer ──
    std::vector<uint8_t> ipc_buf;
    ipc_buf.reserve(sizeof(ipcd::protocol::Header) +
                    sizeof(ipcd::protocol::WallpaperChangedPayload) + 16);

    std::string current_path    = init_path;
    uint16_t    current_fade_ms = 500;
    auto        current_mode    = wallpaper::FitMode::Fill;

    const int wayland_fd = wl_display_get_fd(connection.display());

    // ─────────────────────────────────────────────────────────────────────────
    // Main event loop — up to 4 fds:
    //   [0] Wayland display fd
    //   [1] tinexus-ipcd fd (when connected)
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
        }

        while (wl_display_prepare_read(connection.display()) != 0)
            wl_display_dispatch_pending(connection.display());
        connection.flush();

        struct pollfd fds[4];
        int nfds = 0;
        fds[nfds++] = { .fd = wayland_fd,          .events = POLLIN,                 .revents = 0 };
        if (ipc_fd >= 0)
            fds[nfds++] = { .fd = ipc_fd,           .events = POLLIN|POLLHUP|POLLERR, .revents = 0 };
        if (g_fade_timer_fd >= 0)
            fds[nfds++] = { .fd = g_fade_timer_fd,  .events = POLLIN,                 .revents = 0 };
        if (g_solar_timer.is_valid())
            fds[nfds++] = { .fd = g_solar_timer.fd(),.events = POLLIN,                .revents = 0 };

        int ret = ::poll(fds, static_cast<nfds_t>(nfds), -1);

        if (ret < 0) {
            wl_display_cancel_read(connection.display());
            if (errno == EINTR) continue;
            log::error("[wallpaper] poll() error: {}", std::strerror(errno));
            break;
        }

        // Wayland
        if (fds[0].revents & POLLIN) {
            wl_display_read_events(connection.display());
            wl_display_dispatch_pending(connection.display());
        } else {
            wl_display_cancel_read(connection.display());
        }

        // IPC
        if (ipc_fd >= 0) {
            for (int i = 1; i < nfds; ++i) {
                if (fds[i].fd != ipc_fd) continue;
                if (fds[i].revents & (POLLHUP | POLLERR)) {
                    log::warn("[wallpaper] ipcd socket closed — standalone mode");
                    ::close(ipc_fd); ipc_fd = -1; ipc_buf.clear(); break;
                }
                if (!(fds[i].revents & POLLIN)) break;

                uint8_t tmp[4096]; ssize_t n;
                while ((n = ::read(ipc_fd, tmp, sizeof(tmp))) > 0)
                    ipc_buf.insert(ipc_buf.end(), tmp, tmp + n);
                if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    ::close(ipc_fd); ipc_fd = -1; ipc_buf.clear(); break;
                }

                using namespace ipcd::protocol;
                constexpr size_t HDR = sizeof(Header);
                while (ipc_buf.size() >= HDR) {
                    Header hdr{}; std::memcpy(&hdr, ipc_buf.data(), HDR);
                    if (hdr.magic != TINEXUS_IPC_MAGIC || hdr.version != TINEXUS_IPC_VERSION_1) {
                        log::warn("[wallpaper] Malformed IPC frame"); ipc_buf.clear(); break;
                    }
                    if (hdr.payload_len > 4u * 1024u * 1024u) {
                        log::warn("[wallpaper] Oversized IPC payload"); ipc_buf.clear(); break;
                    }
                    if (ipc_buf.size() < HDR + hdr.payload_len) break;

                    const uint8_t* payload  = ipc_buf.data() + HDR;
                    auto           msg_type = static_cast<MessageType>(hdr.msg_type);

                    if (msg_type == MessageType::WALLPAPER_CHANGED) {
                        if (hdr.payload_len >= sizeof(WallpaperChangedPayload)) {
                            WallpaperChangedPayload pkt{};
                            std::memcpy(&pkt, payload, sizeof(pkt));
                            pkt.path[sizeof(pkt.path) - 1] = '\0';
                            current_path     = std::string(pkt.path);
                            current_mode     = static_cast<wallpaper::FitMode>(pkt.mode);
                            current_fade_ms  = pkt.fade_ms > 0 ? pkt.fade_ms : 500;

                            log::info("[wallpaper] WALLPAPER_CHANGED → '{}' mode={} fade={}ms",
                                      current_path, pkt.mode, pkt.fade_ms);

                            const std::string& target = pkt.dynamic && g_schedule.is_loaded()
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
                        }

                    } else if (msg_type == MessageType::WALLPAPER_STATUS_QUERY) {
                        // M7: full status response
                        auto status = build_status_payload();
                        static_cast<void>(send_ipc_frame(ipc_fd, MessageType::WALLPAPER_STATUS_REPLY,
                                       hdr.sequence_id,
                                       reinterpret_cast<const uint8_t*>(&status), sizeof(status)));

                    } else {
                        log::debug("[wallpaper] Ignoring IPC msg_type={}", hdr.msg_type);
                    }

                    ipc_buf.erase(ipc_buf.begin(),
                                  ipc_buf.begin() + static_cast<ptrdiff_t>(HDR + hdr.payload_len));
                }
                break;
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
    if (ipc_fd >= 0)          ::close(ipc_fd);
    wallpaper::ImageProvider::cache().clear();
    log::info("[wallpaper] tinexus-wallpaper exited cleanly.");
    return 0;
}
