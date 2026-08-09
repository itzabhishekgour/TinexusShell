#ifndef TINEXUS_IPCD_PROTOCOL_INSTALL_HPP
#define TINEXUS_IPCD_PROTOCOL_INSTALL_HPP

#include "header.hpp"
#include <cstdint>
#include <array>

namespace tinexus::ipcd::protocol {

// Struct must be tightly packed
#pragma pack(push, 1)

// Payload for SYS_INSTALL_REQUEST
struct InstallRequestPayload {
    // 64-byte Ed25519 signature of the app payload
    std::array<uint8_t, 64> signature;
    
    // Length of the target app name (e.g. "com.example.app")
    uint16_t app_name_len;
    
    // Note: The actual string data for app_name follows immediately after this struct.
    // The payload file descriptor is sent out-of-band via SCM_RIGHTS ancillary data.
};

// Payload for SYS_INSTALL_OK
// No extra payload required.

// Payload for SYS_INSTALL_FAILED
struct InstallFailedPayload {
    // Error code
    uint32_t error_code;
    // Length of the error message
    uint16_t message_len;
    // The actual error message follows immediately after this struct.
};

#pragma pack(pop)

} // namespace tinexus::ipcd::protocol

#endif // TINEXUS_IPCD_PROTOCOL_INSTALL_HPP
