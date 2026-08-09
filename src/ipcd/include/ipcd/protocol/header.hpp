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

    // 5000+: Config & Themes
    CONFIG_CHANGED          = 5000,
    THEME_CHANGED           = 5001,
};

} // namespace tinexus::ipcd::protocol

#endif // TINEXUS_IPCD_PROTOCOL_HEADER_HPP
