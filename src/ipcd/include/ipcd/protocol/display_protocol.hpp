#pragma once

#include <cstdint>

namespace tinexus::ipcd::protocol {

enum class DisplayMessageType : uint16_t {
    DISPLAY_SET_CONFIG       = 0x0070, // settings-ui -> comp: apply mode/scale/VRR/night-light
    WORKSPACE_STATE_CHANGED  = 0x0071, // comp -> shell: active Space / Fullscreen Space telemetry
    WORKSPACE_SWITCH_REQUEST = 0x0072, // shell -> comp: switch to workspace_id
};

struct __attribute__((packed)) WorkspaceStatePayload {
    uint32_t active_id;
    uint32_t active_slot;      // 1-based visual slot index across ordered Spaces
    uint32_t desktop_count;    // number of desktop Spaces (default 9)
    uint32_t fullscreen_count; // number of active dedicated Fullscreen Spaces
    uint32_t active_spaces;    // count of Spaces with windows or currently active
    uint8_t  is_fullscreen;    // 1 if active Space is a Dedicated Fullscreen Space
    uint8_t  is_transitioning; // 1 while spring slide or swipe gesture is active
    uint8_t  reserved[2];
    char     label[64];        // e.g. "Desktop 1" or "Firefox"
};

static_assert(sizeof(WorkspaceStatePayload) == 88, "WorkspaceStatePayload size mismatch");

struct __attribute__((packed)) WorkspaceSwitchPayload {
    uint32_t workspace_id;
};

static_assert(sizeof(WorkspaceSwitchPayload) == 4, "WorkspaceSwitchPayload size mismatch");

enum DisplayApplyFlags : uint8_t {
    DISPLAY_FLAG_MODE        = 1 << 0,
    DISPLAY_FLAG_SCALE       = 1 << 1,
    DISPLAY_FLAG_VRR         = 1 << 2,
    DISPLAY_FLAG_NIGHT_LIGHT = 1 << 3,
};

struct __attribute__((packed)) DisplayApplyPayload {
    char     connector[64];
    int32_t  width;
    int32_t  height;
    int32_t  refresh_mhz;
    float    scale;
    uint8_t  vrr_enabled;
    uint8_t  night_light;
    uint8_t  flags;
    uint8_t  reserved;
};

static_assert(sizeof(DisplayApplyPayload) == 84, "DisplayApplyPayload size mismatch");

} // namespace tinexus::ipcd::protocol
