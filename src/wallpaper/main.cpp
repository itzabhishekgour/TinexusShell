// ============================================================================
// main.cpp — tinexus-wallpaper (Milestone 3: IPC-Native Event Loop)
// ============================================================================
// Pure C++20 Wayland LAYER_BACKGROUND daemon.
//
// Architecture changes from the previous polling-based design:
//  - REMOVED: /tmp/current_wallpaper file watch (security hole, race-prone)
//  - REMOVED: hardcoded UIDs (/run/user/1000, /run/user/0)
//  - REMOVED: hardcoded usernames (/home/tinexus)
//  - REMOVED: hardcoded 800×600 initial SHM allocation
//  - REMOVED: 250ms blind-poll wallpaper change detection
//  - ADDED:   Connection to tinexus-ipcd on startup
//  - ADDED:   SYS_REGISTER_SERVICE("wallpaper") + SYS_SUBSCRIBE_TOPIC(5002)
//  - ADDED:   Dual-fd poll: Wayland display fd + ipcd socket fd (no spinning)
//  - ADDED:   Instant reload on WALLPAPER_CHANGED (MessageType 5002)
//  - ADDED:   Pure XDG path resolution (no hardcoded paths anywhere)
// ============================================================================

#include "wallpaper/wallpaper_provider.hpp"
#include <txui/wayland/WaylandConnection.hpp>
#include <txui/render/WaylandRenderTarget.hpp>
#include <txui/render/PixmanBackend.hpp>
#include <common/logger.hpp>
#include <ipcd/protocol/header.hpp>

#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#include <wayland-client.h>

#include <sys/socket.h>
#include <sys/un.h>
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

using namespace tinexus;
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Global compositor state
// ─────────────────────────────────────────────────────────────────────────────
static zwlr_layer_shell_v1* g_layer_shell = nullptr;
static std::atomic<bool>    g_running{true};

// Written from signal handler — checked in the main loop
static volatile sig_atomic_t g_reload_requested = 0;

static void registry_handle_global(void*, struct wl_registry* registry,
                                    uint32_t name, const char* interface, uint32_t version) {
    if (std::string_view(interface) == zwlr_layer_shell_v1_interface.name) {
        g_layer_shell = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface,
                             version >= 4 ? 4 : version));
        log::info("[wallpaper] Bound zwlr_layer_shell_v1");
    }
}

static void registry_handle_global_remove(void*, struct wl_registry*, uint32_t) {}

static const struct wl_registry_listener registry_listener = {
    .global        = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

// ─────────────────────────────────────────────────────────────────────────────
// Layer surface configure / close callbacks
// ─────────────────────────────────────────────────────────────────────────────
static bool     g_configured   = false;
static uint32_t g_configured_w = 0;
static uint32_t g_configured_h = 0;

static void layer_surface_configure(void*, struct zwlr_layer_surface_v1* surface,
                                     uint32_t serial, uint32_t width, uint32_t height) {
    zwlr_layer_surface_v1_ack_configure(surface, serial);
    g_configured = true;
    if (width > 0 && height > 0) {
        g_configured_w = width;
        g_configured_h = height;
    }
    log::info("[wallpaper] Compositor configured surface size {}×{}", width, height);
}

static void layer_surface_closed(void*, struct zwlr_layer_surface_v1*) {
    g_running = false;
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed    = layer_surface_closed,
};

// ─────────────────────────────────────────────────────────────────────────────
// Signal handler — SIGUSR1 triggers an immediate soft reload (graceful)
// ─────────────────────────────────────────────────────────────────────────────
static void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        g_reload_requested = 1;
    } else {
        g_running = false;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Path resolution — pure XDG, zero hardcoded paths or UIDs
// ─────────────────────────────────────────────────────────────────────────────

/// Returns the tinexus-ipcd Unix socket path for the current user.
[[nodiscard]] static std::string get_ipc_socket_path() {
    const char* xdg_run = std::getenv("XDG_RUNTIME_DIR");
    if (xdg_run && xdg_run[0] != '\0') {
        return std::string(xdg_run) + "/tinexus/ipc.sock";
    }
    // Derive from uid at runtime — never hardcode 1000 or 0
    return "/run/user/" + std::to_string(static_cast<unsigned>(::getuid()))
           + "/tinexus/ipc.sock";
}

/// Resolves the wallpaper path from settings.toml only.
/// Priority: $XDG_CONFIG_HOME → $HOME/.config → /etc/tinexus (system fallback).
/// Returns empty string if no configured path is found — caller falls back to gradient.
[[nodiscard]] static std::string resolve_wallpaper_path_from_settings() {
    // Build the settings.toml path without any hardcoded username
    fs::path cfg_path;
    const char* xdg_cfg = std::getenv("XDG_CONFIG_HOME");
    if (xdg_cfg && xdg_cfg[0] != '\0') {
        cfg_path = fs::path(xdg_cfg) / "tinexus/settings.toml";
    } else {
        const char* home = std::getenv("HOME");
        if (home && home[0] != '\0') {
            cfg_path = fs::path(home) / ".config/tinexus/settings.toml";
        } else {
            cfg_path = "/etc/tinexus/settings.toml";
        }
    }

    std::error_code ec;
    if (!fs::exists(cfg_path, ec) || ec) {
        return "";
    }

    // Minimal line-by-line TOML parse for [wallpaper] / path = "..."
    // A full TOML parser belongs to tinexus-settings; we only need this one key.
    std::ifstream f(cfg_path);
    bool in_wallpaper_section = false;
    std::string line;
    while (std::getline(f, line)) {
        // Strip leading whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        if (line == "[wallpaper]") {
            in_wallpaper_section = true;
            continue;
        }
        // Any other section header exits the wallpaper block
        if (!line.empty() && line[0] == '[') {
            in_wallpaper_section = false;
            continue;
        }

        if (in_wallpaper_section && line.starts_with("path")) {
            auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string val = line.substr(eq + 1);
            // Strip surrounding whitespace and quotes
            val.erase(0, val.find_first_not_of(" \t\"'"));
            val.erase(val.find_last_not_of(" \t\"'\r\n") + 1);
            if (!val.empty()) {
                // Validate the resolved path actually exists
                if (fs::exists(val, ec) && !ec) {
                    return val;
                }
                // Try XDG_DATA_DIRS as a secondary resolution for system-installed packs
                const char* data_dirs = std::getenv("XDG_DATA_DIRS");
                if (data_dirs) {
                    std::string dirs(data_dirs);
                    size_t pos = 0;
                    while (pos < dirs.size()) {
                        size_t sep = dirs.find(':', pos);
                        std::string dir = dirs.substr(pos, sep == std::string::npos ? sep : sep - pos);
                        fs::path candidate = fs::path(dir) / "tinexus/backgrounds" / fs::path(val).filename();
                        if (fs::exists(candidate, ec) && !ec) {
                            return candidate.string();
                        }
                        if (sep == std::string::npos) break;
                        pos = sep + 1;
                    }
                }
            }
        }
    }
    return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// IPC client helpers — lightweight, no external library
// ─────────────────────────────────────────────────────────────────────────────

/// Connect to tinexus-ipcd. Returns blocking socket fd >= 0, or -1 on failure.
[[nodiscard]] static int connect_to_ipcd() {
    const std::string path = get_ipc_socket_path();
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        log::error("[wallpaper] socket() failed: {}", std::strerror(errno));
        return -1;
    }
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        log::warn("[wallpaper] ipcd not reachable at '{}': {} "
                  "(will run in standalone mode, reloading from settings.toml on SIGUSR1)",
                  path, std::strerror(errno));
        ::close(fd);
        return -1;
    }
    log::info("[wallpaper] Connected to tinexus-ipcd at '{}'", path);
    return fd;
}

/// Send a complete IPC frame (Header + payload) on a blocking socket.
[[nodiscard]] static bool send_ipc_frame(int fd,
                                          ipcd::protocol::MessageType msg_type,
                                          uint32_t sequence_id,
                                          const uint8_t* payload,
                                          uint32_t payload_len) {
    using namespace ipcd::protocol;
    Header hdr{};
    hdr.magic       = TINEXUS_IPC_MAGIC;
    hdr.version     = TINEXUS_IPC_VERSION_1;
    hdr.msg_type    = static_cast<uint16_t>(msg_type);
    hdr.flags       = 0;
    hdr.sequence_id = sequence_id;
    hdr.payload_len = payload_len;
    hdr.checksum    = 0;

    std::vector<uint8_t> buf;
    buf.resize(sizeof(hdr) + payload_len);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    if (payload_len > 0 && payload != nullptr) {
        std::memcpy(buf.data() + sizeof(hdr), payload, payload_len);
    }

    const uint8_t* ptr = buf.data();
    size_t remaining   = buf.size();
    while (remaining > 0) {
        ssize_t n = ::write(fd, ptr, remaining);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        ptr       += static_cast<size_t>(n);
        remaining -= static_cast<size_t>(n);
    }
    return true;
}

/// Register with ipcd as service "wallpaper" and subscribe to WALLPAPER_CHANGED (5002).
/// Returns true if the handshake frames were sent successfully.
static bool register_with_ipcd(int ipc_fd) {
    using MT = ipcd::protocol::MessageType;

    // 1. SYS_REGISTER_SERVICE — payload: "wallpaper\0"
    const char* svc_name = "wallpaper";
    if (!send_ipc_frame(ipc_fd, MT::SYS_REGISTER_SERVICE, 1,
                        reinterpret_cast<const uint8_t*>(svc_name),
                        static_cast<uint32_t>(std::strlen(svc_name) + 1))) {
        log::error("[wallpaper] Failed to send SYS_REGISTER_SERVICE");
        return false;
    }

    // 2. SYS_SUBSCRIBE_TOPIC — payload: uint16_t topic id = 5002 (WALLPAPER_CHANGED)
    uint16_t topic = static_cast<uint16_t>(MT::WALLPAPER_CHANGED);
    if (!send_ipc_frame(ipc_fd, MT::SYS_SUBSCRIBE_TOPIC, 2,
                        reinterpret_cast<const uint8_t*>(&topic), sizeof(topic))) {
        log::error("[wallpaper] Failed to send SYS_SUBSCRIBE_TOPIC");
        return false;
    }

    log::info("[wallpaper] Registered as 'wallpaper' service; subscribed to WALLPAPER_CHANGED ({})",
              static_cast<uint16_t>(MT::WALLPAPER_CHANGED));
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    // Install signal handlers — SIGUSR1 for soft reload, SIGTERM/SIGINT for shutdown
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; // No SA_RESTART: poll() must return EINTR on SIGUSR1
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGUSR1, &sa, nullptr);

    log::set_component_name("wallpaper");
    log::info("[wallpaper] tinexus-wallpaper starting (Milestone 3: IPC-native)...");

    // ── Connect to tinexus-ipcd (non-fatal: daemon runs in standalone mode if ipcd is down) ──
    int ipc_fd = connect_to_ipcd();
    if (ipc_fd >= 0) {
        if (!register_with_ipcd(ipc_fd)) {
            log::warn("[wallpaper] ipcd handshake failed; running in standalone mode");
            ::close(ipc_fd);
            ipc_fd = -1;
        }
    }

    // ── Connect to Wayland compositor ──
    auto conn_opt = txui::wayland::WaylandConnection::connect();
    if (!conn_opt) {
        log::error("[wallpaper] Failed to connect to Wayland display!");
        if (ipc_fd >= 0) ::close(ipc_fd);
        return 1;
    }
    auto connection = std::move(*conn_opt);

    wl_registry* reg = wl_display_get_registry(connection.display());
    wl_registry_add_listener(reg, &registry_listener, nullptr);
    wl_display_roundtrip(connection.display());

    if (!g_layer_shell) {
        log::error("[wallpaper] Compositor does not support zwlr_layer_shell_v1!");
        if (ipc_fd >= 0) ::close(ipc_fd);
        return 1;
    }

    // ── Create Wayland SHM render target — initial size 1×1; compositor tells us the real size ──
    // Allocating 800×600 before configure was incorrect: on a 4K display this wastes a roundtrip.
    auto target_opt = txui::WaylandRenderTarget::create(connection, 1, 1);
    if (!target_opt) {
        log::error("[wallpaper] Failed to create WaylandRenderTarget!");
        if (ipc_fd >= 0) ::close(ipc_fd);
        return 1;
    }
    auto render_target = std::make_unique<txui::WaylandRenderTarget>(std::move(*target_opt));

    wl_surface* raw_surface = render_target->surface().surface();

    // ── Create layer surface on LAYER_BACKGROUND for all outputs (nullptr = default output) ──
    // Note: Multi-output support (per-output surface map) is Milestone 1. For now, nullptr
    // creates a surface on the primary/default output — this is intentional for this milestone.
    struct zwlr_layer_surface_v1* layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        g_layer_shell, raw_surface, nullptr,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "tinexus-wallpaper");

    zwlr_layer_surface_v1_add_listener(layer_surface, &layer_surface_listener, nullptr);
    zwlr_layer_surface_v1_set_size(layer_surface, 0, 0); // 0,0 = fill output
    zwlr_layer_surface_v1_set_anchor(layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP    |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT   |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1); // Don't reserve any space

    wl_surface_commit(raw_surface);
    connection.flush();
    connection.roundtrip();

    // ── Wait for initial configure event before allocating the real-sized buffer ──
    while (!g_configured && g_running) {
        if (wl_display_dispatch(connection.display()) == -1) break;
    }
    if (!g_running) {
        zwlr_layer_surface_v1_destroy(layer_surface);
        if (ipc_fd >= 0) ::close(ipc_fd);
        return 0;
    }

    // Resize render target to compositor-provided dimensions
    uint32_t w = g_configured_w > 0 ? g_configured_w : 1920;
    uint32_t h = g_configured_h > 0 ? g_configured_h : 1080;
    render_target->resize(w, h);
    log::info("[wallpaper] Surface configured at {}×{}", w, h);

    // ── Wallpaper render helper ──
    wallpaper::ImageProvider provider;

    // Tracks the last path successfully committed so we don't re-render unchanged images
    std::string last_committed_path;

    auto render_and_commit = [&](const std::string& override_path = "") {
        // Path priority: IPC override → settings.toml → procedural gradient
        std::string path = override_path.empty()
                           ? resolve_wallpaper_path_from_settings()
                           : override_path;

        if (!path.empty() && path != last_committed_path) {
            if (provider.load(path)) {
                log::info("[wallpaper] Loaded '{}'", path);
            } else {
                log::warn("[wallpaper] Failed to load '{}'; using procedural gradient", path);
                path = ""; // Force gradient fallback
            }
        } else if (path.empty()) {
            // No path → procedural gradient is rendered by provider.render_buffer()
        }

        wallpaper::WallpaperBuffer buf = provider.render_buffer(w, h);
        if (render_target->data() && !buf.pixels.empty()) {
            // Synchronize both double-buffers in the swapchain
            std::copy(buf.pixels.begin(), buf.pixels.end(), render_target->data());
            render_target->present();
            std::copy(buf.pixels.begin(), buf.pixels.end(), render_target->data());
            render_target->present();
            connection.flush();
            connection.roundtrip();
            last_committed_path = path;
            log::info("[wallpaper] Committed to LAYER_BACKGROUND ({}×{})", w, h);
        }
    };

    // ── Initial render ──
    render_and_commit();

    // ── IPC message receive buffer ──
    // We accumulate bytes from the ipcd fd here between wakeups
    std::vector<uint8_t> ipc_buf;
    ipc_buf.reserve(sizeof(ipcd::protocol::Header) + sizeof(ipcd::protocol::WallpaperChangedPayload) + 16);

    // ─────────────────────────────────────────────────────────────────────────
    // Main event loop — dual-fd poll:
    //   fd[0]: Wayland display fd  (compositor events)
    //   fd[1]: tinexus-ipcd fd     (WALLPAPER_CHANGED notifications)
    //
    // No timeout spinning. poll() blocks indefinitely until:
    //   - A Wayland event arrives (ping, output change, close)
    //   - ipcd delivers a WALLPAPER_CHANGED message
    //   - A signal interrupts (SIGUSR1 → soft reload, SIGTERM → clean shutdown)
    // ─────────────────────────────────────────────────────────────────────────
    const int wayland_fd = wl_display_get_fd(connection.display());

    while (g_running) {
        // Check soft-reload signal (from SIGUSR1 or test harness)
        if (g_reload_requested) {
            g_reload_requested = 0;
            log::info("[wallpaper] SIGUSR1 received — reloading from settings.toml");
            render_and_commit();
        }

        // Prepare Wayland for read (must call before poll to avoid race)
        while (wl_display_prepare_read(connection.display()) != 0) {
            wl_display_dispatch_pending(connection.display());
        }
        connection.flush();

        // Build pollfd array — Wayland always present; ipcd only if connected
        struct pollfd fds[2];
        int nfds = 0;

        fds[nfds++] = { .fd = wayland_fd, .events = POLLIN, .revents = 0 };
        if (ipc_fd >= 0) {
            fds[nfds++] = { .fd = ipc_fd, .events = POLLIN | POLLHUP | POLLERR, .revents = 0 };
        }

        // Block until activity or signal — no arbitrary timeout
        int ret = ::poll(fds, static_cast<nfds_t>(nfds), -1);

        if (ret < 0) {
            wl_display_cancel_read(connection.display());
            if (errno == EINTR) continue; // Signal interrupted — loop back to check g_reload_requested
            log::error("[wallpaper] poll() error: {}", std::strerror(errno));
            break;
        }

        // ── Handle Wayland events ──
        if (fds[0].revents & POLLIN) {
            wl_display_read_events(connection.display());
            wl_display_dispatch_pending(connection.display());
        } else {
            wl_display_cancel_read(connection.display());
        }

        // ── Handle ipcd events ──
        if (ipc_fd >= 0 && nfds > 1) {
            if (fds[1].revents & (POLLHUP | POLLERR)) {
                log::warn("[wallpaper] ipcd socket closed (daemon may have restarted); "
                          "will use SIGUSR1 for reloads until reconnect is implemented");
                ::close(ipc_fd);
                ipc_fd = -1;
                ipc_buf.clear();
                continue;
            }

            if (fds[1].revents & POLLIN) {
                // Drain the socket into ipc_buf
                uint8_t tmp[4096];
                ssize_t n;
                while ((n = ::read(ipc_fd, tmp, sizeof(tmp))) > 0) {
                    ipc_buf.insert(ipc_buf.end(), tmp, tmp + static_cast<size_t>(n));
                }
                if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
                    log::warn("[wallpaper] ipcd read error: {} — entering standalone mode",
                              std::strerror(errno));
                    ::close(ipc_fd);
                    ipc_fd = -1;
                    ipc_buf.clear();
                    continue;
                }

                // Parse all complete frames from ipc_buf
                using namespace ipcd::protocol;
                constexpr size_t HDR_SZ = sizeof(Header);

                while (ipc_buf.size() >= HDR_SZ) {
                    Header hdr{};
                    std::memcpy(&hdr, ipc_buf.data(), HDR_SZ);

                    // Validate magic + version
                    if (hdr.magic != TINEXUS_IPC_MAGIC || hdr.version != TINEXUS_IPC_VERSION_1) {
                        log::warn("[wallpaper] Received malformed IPC frame — discarding buffer");
                        ipc_buf.clear();
                        break;
                    }

                    // Guard against oversized payloads
                    if (hdr.payload_len > 4u * 1024u * 1024u) {
                        log::warn("[wallpaper] IPC payload too large ({} bytes) — discarding", hdr.payload_len);
                        ipc_buf.clear();
                        break;
                    }

                    if (ipc_buf.size() < HDR_SZ + hdr.payload_len) {
                        break; // Incomplete frame — wait for more data
                    }

                    const uint8_t* payload_ptr = ipc_buf.data() + HDR_SZ;
                    auto msg_type = static_cast<MessageType>(hdr.msg_type);

                    if (msg_type == MessageType::WALLPAPER_CHANGED) {
                        // Payload must be exactly sizeof(WallpaperChangedPayload) == 524 bytes
                        if (hdr.payload_len >= sizeof(WallpaperChangedPayload)) {
                            WallpaperChangedPayload pkt{};
                            std::memcpy(&pkt, payload_ptr, sizeof(pkt));

                            // Force null-termination regardless of what we received
                            pkt.path[sizeof(pkt.path) - 1] = '\0';

                            std::string new_path(pkt.path);
                            log::info("[wallpaper] WALLPAPER_CHANGED received — path='{}', mode={}, fade_ms={}",
                                      new_path, pkt.mode, pkt.fade_ms);

                            // Trigger reload with the new path from the IPC payload
                            // Cross-fade (pkt.fade_ms) will be wired up in Milestone 4
                            render_and_commit(new_path);
                        } else {
                            log::warn("[wallpaper] WALLPAPER_CHANGED payload too small ({} bytes)", hdr.payload_len);
                        }
                    } else if (msg_type == MessageType::WALLPAPER_STATUS_QUERY) {
                        // Settings UI is asking for our current state — reply with current path
                        // (Full status reply struct will be expanded in Milestone 7)
                        const std::string& cur = last_committed_path;
                        send_ipc_frame(ipc_fd, MessageType::WALLPAPER_STATUS_REPLY, hdr.sequence_id,
                                       reinterpret_cast<const uint8_t*>(cur.c_str()),
                                       static_cast<uint32_t>(cur.size() + 1));
                    } else {
                        log::debug("[wallpaper] Ignoring unexpected IPC msg_type={}", hdr.msg_type);
                    }

                    // Consume the processed frame from the buffer
                    ipc_buf.erase(ipc_buf.begin(),
                                  ipc_buf.begin() + static_cast<ptrdiff_t>(HDR_SZ + hdr.payload_len));
                }
            }
        }
    }

    // ── Clean shutdown ──
    log::info("[wallpaper] Shutting down...");
    if (layer_surface) zwlr_layer_surface_v1_destroy(layer_surface);
    if (ipc_fd >= 0)   ::close(ipc_fd);
    log::info("[wallpaper] tinexus-wallpaper exited cleanly.");
    return 0;
}

