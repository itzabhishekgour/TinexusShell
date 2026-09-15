#ifndef TINEXUS_IPCD_PROTOCOL_HEADER_HPP
#define TINEXUS_IPCD_PROTOCOL_HEADER_HPP

#include <cstdint>

namespace tinexus::ipcd::protocol {

constexpr uint32_t TINEXUS_IPC_MAGIC = 0x544E5853; // "TNXS"
constexpr uint16_t TINEXUS_IPC_VERSION_1 = 0x0100;

// Struct must be tightly packed
#pragma pack(push, 1)
struct Header {
    uint32_t magic;         // 0x544E5853 ("TNXS")
    uint16_t version;       // Protocol version (e.g. 0x0100)
    uint16_t msg_type;      // Message ID (see MessageType)
    uint16_t flags;         // Reserved flags
    uint32_t sequence_id;   // Monotonic request ID
    uint32_t payload_len;   // Payload length in bytes
    uint32_t checksum;      // CRC32 checksum of payload (optional)
};
#pragma pack(pop)

static_assert(sizeof(Header) == 22, "IPC Header must be exactly 22 bytes");

// ─────────────────────────────────────────────────────────────────────────────
// WallpaperChangedPayload — wire format for WALLPAPER_CHANGED (5002)
//
// Sent by tinexus-settings when the user selects a new wallpaper.
// Broadcast by tinexus-ipcd to all topic-5002 subscribers:
//   → tinexus-wallpaper  (renders LAYER_BACKGROUND)
//   → tinexus-lock       (renders blurred wallpaper on LAYER_LOCK)
//
// All fields are fixed-size. No heap allocation. No JSON. Safe to memcpy
// directly from the raw socket receive buffer.
//
// Wire size: 512 + 1 + 1 + 2 + 8 = 524 bytes exactly.
// ─────────────────────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct WallpaperChangedPayload {
    // Absolute, null-terminated UTF-8 path to the wallpaper asset.
    // An empty string (path[0] == '\0') means "use the active dynamic schedule."
    char     path[512];

    // Fit mode for how the image maps to the output surface:
    //   0 = fill   (aspect-fill, crops edges to cover entire output — default)
    //   1 = fit    (aspect-fit, letterbox/pillarbox to avoid any cropping)
    //   2 = center (natural image size, centered, no scaling)
    //   3 = tile   (tiled at 1:1 pixel size across the output)
    //   4 = stretch (unconstrained stretch — distorts, use sparingly)
    uint8_t  mode;

    // Dynamic schedule flag:
    //   0 = static image (path points to a single image file)
    //   1 = dynamic (.twallpaper bundle; path points to manifest directory)
    uint8_t  dynamic;

    // Cross-fade duration when transitioning from the current wallpaper.
    //   0     = instant swap (no animation)
    //   500   = recommended smooth transition (500ms, 30 frames at 60fps)
    //   65535 = maximum allowed fade (65.5 seconds)
    uint16_t fade_ms;

    // Reserved for future expansion (v2 HDR flags, per-output targeting, etc.).
    // Sender MUST zero all 8 bytes. Receiver MUST ignore unknown bits.
    uint8_t  reserved[8];
};
#pragma pack(pop)

static_assert(sizeof(WallpaperChangedPayload) == 524,
              "WallpaperChangedPayload wire size must be exactly 524 bytes");

// Reserved Message IDs for Service Routing and Pub/Sub
enum class MessageType : uint16_t {
    // 0-999: System & Broker Control
    SYS_REGISTER_SERVICE    = 0,
    SYS_UNREGISTER_SERVICE  = 1,
    SYS_SUBSCRIBE_TOPIC     = 2,
    SYS_UNSUBSCRIBE_TOPIC   = 3,
    SYS_DISCOVER_SERVICE    = 4,
    SYS_SERVICE_EVENT       = 5,
    SYS_INSTALL_REQUEST     = 6,
    SYS_INSTALL_OK          = 7,
    SYS_INSTALL_FAILED      = 8,
    SYS_UNINSTALL_REQUEST   = 9,
    SYS_UNINSTALL_OK        = 10,
    SYS_UNINSTALL_FAILED    = 11,
    SYS_PING                = 12,
    SYS_PONG                = 13,
    SYS_SHUTDOWN            = 999,

    // 1000-1999: Launcher & Input Shortcuts
    LAUNCHER_OPEN           = 1000,
    LAUNCHER_CLOSE          = 1001,
    LAUNCHER_APP_LAUNCHED   = 1002,
    LAUNCHER_OPEN_FILE      = 1003,
    LAUNCHER_SHOW           = 1004,
    SHORTCUT_ACTIVATED      = 1005,
    ACTION_REQUEST          = 1006,

    // 2000-2999: Search Engine
    SEARCH_QUERY            = 2000,
    SEARCH_RESULT           = 2001,
    SEARCH_CLEAR            = 2002,

    // 3000-3999: Clipboard
    CLIPBOARD_CHANGED       = 3000,
    CLIPBOARD_GET_HISTORY   = 3001,
    CLIPBOARD_SET_ITEM      = 3002,

    // 4000-4999: Plugin System
    PLUGIN_INIT             = 4000,
    PLUGIN_DATA             = 4001,
    PLUGIN_STOP             = 4002,

    // 5000+: Config, Themes & Wallpaper
    CONFIG_CHANGED          = 5000,
    THEME_CHANGED           = 5001,
    WALLPAPER_CHANGED       = 5002,
    WALLPAPER_STATUS_QUERY  = 5003,
    WALLPAPER_STATUS_REPLY  = 5004,
    WALLPAPER_FRAME_ADVANCE = 5005,
};

// ─────────────────────────────────────────────────────────────────────────────
// WallpaperStatusPayload — wire format for WALLPAPER_STATUS_REPLY (5004)
//
// Sent by tinexus-wallpaper in response to a WALLPAPER_STATUS_QUERY (5003).
// Enables the Settings UI to render an accurate thumbnail and display current
// schedule state without polling a file or calling a D-Bus method.
//
// Wire size: 512 + 1 + 1 + 2 + 2 + 4 + 4 = 526 bytes.
// ─────────────────────────────────────────────────────────────────────────────
#pragma pack(push, 1)
struct WallpaperStatusPayload {
    // Absolute UTF-8 path to the currently committed wallpaper asset.
    // Empty (path[0] == '\0') means a dynamic schedule is active and the path
    // of the current frame is encoded in current_frame_index.
    char     path[512];

    // Fit mode currently applied (matches WallpaperChangedPayload::mode encoding).
    uint8_t  mode;

    // 0 = static image active; 1 = dynamic .twallpaper schedule active.
    uint8_t  is_dynamic;

    // Index of the currently displayed frame (0 for static, 0–N for dynamic).
    uint16_t current_frame_index;

    // Total number of frames in the active schedule (1 for static images).
    uint16_t total_frames;

    // Seconds until the next automatic frame change.
    // 0 if is_dynamic == 0 or no next transition is scheduled.
    uint32_t next_change_secs;

    // Reserved for v2 (HDR status, color space, per-output state).
    // Must be zeroed by the sender. Receiver must ignore unknown bits.
    uint8_t  reserved[4];
};
#pragma pack(pop)

static_assert(sizeof(WallpaperStatusPayload) == 526,
              "WallpaperStatusPayload wire size must be exactly 526 bytes");

} // namespace tinexus::ipcd::protocol

#endif // TINEXUS_IPCD_PROTOCOL_HEADER_HPP
