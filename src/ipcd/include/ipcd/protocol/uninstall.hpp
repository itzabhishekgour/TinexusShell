#ifndef TINEXUS_IPCD_PROTOCOL_UNINSTALL_HPP
#define TINEXUS_IPCD_PROTOCOL_UNINSTALL_HPP

#include <cstdint>

namespace tinexus::ipcd::protocol {

// Payload for SYS_UNINSTALL_REQUEST
#pragma pack(push, 1)
struct UninstallRequestPayload {
    char app_name[64];
};
#pragma pack(pop)

static_assert(sizeof(UninstallRequestPayload) == 64, "UninstallRequestPayload must be exactly 64 bytes");

} // namespace tinexus::ipcd::protocol

#endif // TINEXUS_IPCD_PROTOCOL_UNINSTALL_HPP
