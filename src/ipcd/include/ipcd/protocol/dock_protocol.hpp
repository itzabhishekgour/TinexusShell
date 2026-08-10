#pragma once

#include <cstdint>

namespace tinexus::ipcd::protocol {

enum class DockMessageType : uint16_t {
    DOCK_QUERY_ICON_POSITION  = 0x0060,  // comp→dock: "where is icon for app_id?"
    DOCK_ICON_POSITION        = 0x0061,  // dock→comp: "icon at x,y,w,h"
    DOCK_NOTIFY_MINIMIZED     = 0x0062,  // comp→dock: "window minimized"
    DOCK_NOTIFY_RESTORED      = 0x0063,  // comp→dock: "window restored"
    DOCK_RESTORE_REQUEST      = 0x0064,  // dock→comp: "restore this window"
    DOCK_RAISE_AND_FOCUS      = 0x0065,  // dock→comp: "raise+focus running-bg window"
    DOCK_NOTIFY_FOCUS_CHANGED = 0x0066,  // comp→dock: "app focus state changed"
};

struct DockQueryIconPositionPayload {
    char app_id[128];
};

struct DockIconPositionPayload {
    char app_id[128];
    int32_t x{0};
    int32_t y{0};
    int32_t w{0};
    int32_t h{0};
};

struct DockNotifyPayload {
    char app_id[128];
    uint64_t surface_id{0};
};

struct DockFocusChangedPayload {
    char     app_id[128];
    uint8_t  is_focused{0};   // 1 = became focused, 0 = lost focus
};

} // namespace tinexus::ipcd::protocol
