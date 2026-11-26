#pragma once

#include <cstdint>

namespace tinexus::ipcd::protocol {

enum class DisplayMessageType : uint16_t {
    DISPLAY_SET_CONFIG = 0x0070, // settings-ui -> comp: apply mode/scale/VRR/night-light
};

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
