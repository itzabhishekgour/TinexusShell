// ============================================================================
// settings_daemon.cpp — tinexus-settings
// ============================================================================
// Manages live platform configuration and broadcasts changes to tinexus-ipcd
// so all subscribed daemons (wallpaper, lock, shell) react instantly.
// ============================================================================
#include "settings/settings_daemon.hpp"
#include "settings/schema_validator.hpp"
#include "common/logger.hpp"

#include <ipcd/protocol/header.hpp>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
#include <cerrno>

namespace tinexus::settings {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers — anonymous namespace, not part of public API
// ─────────────────────────────────────────────────────────────────────────────
namespace {

/// Returns the ipcd Unix socket path for the current user.
/// Pure XDG resolution — no hardcoded UIDs or /tmp paths.
[[nodiscard]] std::string get_ipc_socket_path() {
    const char* xdg_run = std::getenv("XDG_RUNTIME_DIR");
    if (xdg_run && xdg_run[0] != '\0') {
        return std::string(xdg_run) + "/tinexus/ipc.sock";
    }
    // Fallback: derive from uid at runtime (avoids hardcoding 1000 or 0)
    uid_t uid = ::getuid();
    return "/run/user/" + std::to_string(uid) + "/tinexus/ipc.sock";
}

/// Open a blocking SOCK_STREAM connection to tinexus-ipcd.
/// Returns fd >= 0 on success, -1 on failure (logs the error internally).
[[nodiscard]] int connect_to_ipcd() {
    const std::string path = get_ipc_socket_path();
    int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        log::error("[settings] socket() failed: {}", std::strerror(errno));
        return -1;
    }

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    ::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        log::warn("[settings] connect to ipcd at '{}' failed: {} "
                  "(ipcd may not be running yet; settings were still saved to disk)",
                  path, std::strerror(errno));
        ::close(fd);
        return -1;
    }
    return fd;
}

/// Write a complete IPC frame (Header + payload) to a blocking socket fd.
/// Retries on EINTR. Returns true on full write, false on any error.
[[nodiscard]] bool write_ipc_frame(int fd,
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
    hdr.checksum    = 0; // CRC32 reserved for protocol v1.1

    // Build a single contiguous buffer: header || payload
    std::vector<uint8_t> buf;
    buf.resize(sizeof(hdr) + payload_len);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    if (payload_len > 0 && payload != nullptr) {
        std::memcpy(buf.data() + sizeof(hdr), payload, payload_len);
    }

    // Blocking write with EINTR restart
    const uint8_t* ptr = buf.data();
    size_t remaining   = buf.size();
    while (remaining > 0) {
        ssize_t written = ::write(fd, ptr, remaining);
        if (written < 0) {
            if (errno == EINTR) continue;
            log::error("[settings] IPC write() failed: {}", std::strerror(errno));
            return false;
        }
        ptr       += static_cast<size_t>(written);
        remaining -= static_cast<size_t>(written);
    }
    return true;
}

/// Resolve the settings.toml path cleanly, with no hardcoded usernames.
[[nodiscard]] std::filesystem::path resolve_config_path() {
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::filesystem::path(home) / ".config/tinexus/settings.toml";
    }
    // System-wide fallback — no username, no hardcoded UID
    return std::filesystem::path("/etc/tinexus/settings.toml");
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// SettingsDaemon — public interface
// ─────────────────────────────────────────────────────────────────────────────

SettingsDaemon& SettingsDaemon::instance() noexcept {
    static SettingsDaemon s_instance;
    return s_instance;
}

bool SettingsDaemon::initialize(const std::filesystem::path& config_path) {
    log::info("SettingsDaemon: Initializing with config '{}'...", config_path.string());
    return ConfigStore::instance().load_settings(config_path);
}

// ─────────────────────────────────────────────────────────────────────────────
// broadcast_settings_changed — REAL implementation (was a dead stub)
//
// For generic categories (appearance, display, input) we send CONFIG_CHANGED
// or THEME_CHANGED to ipcd which pub-sub broadcasts to all subscribers.
// Wallpaper changes must go through update_wallpaper() for the typed payload.
// ─────────────────────────────────────────────────────────────────────────────
void SettingsDaemon::broadcast_settings_changed(const std::string& category) {
    using MT = ipcd::protocol::MessageType;

    log::info("[settings] Broadcasting settings change for category '{}'", category);

    MT msg_type = MT::CONFIG_CHANGED;
    if (category == "appearance" || category == "theme") {
        msg_type = MT::THEME_CHANGED;
    }

    int ipc_fd = connect_to_ipcd();
    if (ipc_fd < 0) {
        log::warn("[settings] ipcd unreachable; '{}' change will not propagate live", category);
        return;
    }

    // Payload: null-terminated category string so subscribers can filter if needed
    std::vector<uint8_t> payload(category.begin(), category.end());
    payload.push_back('\0');

    static uint32_t s_seq = 0;
    bool ok = write_ipc_frame(ipc_fd, msg_type, ++s_seq,
                              payload.data(), static_cast<uint32_t>(payload.size()));
    ::close(ipc_fd);

    if (ok) {
        log::info("[settings] '{}' change broadcast sent (msg_type={})",
                  category, static_cast<uint16_t>(msg_type));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// update_wallpaper — live wallpaper change with instant IPC propagation
//
// Steps:
//  1. Validate path exists on disk (empty path = use dynamic schedule).
//  2. Update ConfigStore + atomically write settings.toml (write→fsync→rename).
//  3. Build WallpaperChangedPayload (524 bytes), zero all reserved bytes.
//  4. Connect to ipcd, send WALLPAPER_CHANGED frame, close socket.
//     ipcd broadcasts to all subscribers — zero daemon restarts needed.
// ─────────────────────────────────────────────────────────────────────────────
bool SettingsDaemon::update_wallpaper(const std::string& new_path,
                                      uint8_t  mode,
                                      uint16_t fade_ms,
                                      bool     is_dynamic) {
    using namespace ipcd::protocol;

    // 1. Validate — non-empty paths must exist on disk right now
    if (!new_path.empty()) {
        std::error_code ec;
        if (!std::filesystem::exists(new_path, ec) || ec) {
            log::error("[settings] update_wallpaper: path not found: '{}'", new_path);
            return false;
        }
    }

    // 2. Persist to ConfigStore and atomically write settings.toml
    auto settings = ConfigStore::instance().get_settings();
    if (!new_path.empty()) {
        settings.wallpaper_path = new_path;
    }
    const char* mode_strings[] = {"fill", "fit", "center", "tile", "stretch"};
    settings.wallpaper_mode = (mode < 5) ? mode_strings[mode] : "fill";
    ConfigStore::instance().update_settings(settings);

    if (!ConfigStore::instance().save_settings_atomic(resolve_config_path())) {
        log::error("[settings] Atomic write of settings.toml failed after wallpaper change");
        return false;
    }

    // 3. Build WallpaperChangedPayload — EXACTLY 524 bytes on the wire
    WallpaperChangedPayload pkt{};
    std::memset(&pkt, 0, sizeof(pkt)); // Zeroes reserved[] and all padding

    if (!new_path.empty()) {
        // Guaranteed null-termination: strncpy copies up to 511 chars, [511] stays '\0'
        std::strncpy(pkt.path, new_path.c_str(), sizeof(pkt.path) - 1);
    }
    pkt.mode    = mode;
    pkt.dynamic = is_dynamic ? static_cast<uint8_t>(1) : static_cast<uint8_t>(0);
    pkt.fade_ms = fade_ms;

    // 4. Send to tinexus-ipcd — connect, send, close (fire-and-forget)
    int ipc_fd = connect_to_ipcd();
    if (ipc_fd < 0) {
        // Non-fatal: settings are persisted; daemon will reload from TOML on next restart
        log::warn("[settings] WALLPAPER_CHANGED not sent (ipcd unreachable); "
                  "settings saved, wallpaper will apply on next daemon restart");
        return false;
    }

    static uint32_t s_seq = 1000;
    bool ok = write_ipc_frame(ipc_fd, MessageType::WALLPAPER_CHANGED, ++s_seq,
                              reinterpret_cast<const uint8_t*>(&pkt),
                              static_cast<uint32_t>(sizeof(pkt)));
    ::close(ipc_fd);

    if (ok) {
        log::info("[settings] WALLPAPER_CHANGED sent (path='{}', mode={}, fade_ms={}, dynamic={})",
                  new_path, mode, fade_ms, is_dynamic ? 1 : 0);
    }
    return ok;
}

bool SettingsDaemon::update_theme(const std::string& new_theme) {
    if (!SchemaValidator::validate_theme(new_theme)) {
        log::error("SettingsDaemon: Invalid theme '{}' rejected by SchemaValidator", new_theme);
        return false;
    }

    auto settings = ConfigStore::instance().get_settings();
    settings.theme = new_theme;
    ConfigStore::instance().update_settings(settings);

    if (ConfigStore::instance().save_settings_atomic(resolve_config_path())) {
        broadcast_settings_changed("appearance");
        return true;
    }
    return false;
}

bool SettingsDaemon::update_scale(float new_scale) {
    if (!SchemaValidator::validate_scale(new_scale)) {
        log::error("SettingsDaemon: Invalid scale '{}' rejected by SchemaValidator", new_scale);
        return false;
    }

    auto settings = ConfigStore::instance().get_settings();
    settings.display_scale = new_scale;
    ConfigStore::instance().update_settings(settings);

    if (ConfigStore::instance().save_settings_atomic(resolve_config_path())) {
        broadcast_settings_changed("display");
        return true;
    }
    return false;
}

} // namespace tinexus::settings
