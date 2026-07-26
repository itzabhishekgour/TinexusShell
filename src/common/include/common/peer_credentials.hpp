#ifndef TINEXUS_COMMON_PEER_CREDENTIALS_HPP
#define TINEXUS_COMMON_PEER_CREDENTIALS_HPP

#include <sys/types.h>
#include "common/result.hpp"

namespace tinexus::security {

struct PeerCredentials {
    pid_t pid;
    uid_t uid;
    gid_t gid;
};

enum class SecurityError {
    SocketOptionFailed,
    PermissionDenied,
    InvalidSocket
};

Result<PeerCredentials, SecurityError> get_peer_credentials(int socket_fd) noexcept;
bool verify_same_user(int socket_fd) noexcept;

} // namespace tinexus::security

#endif // TINEXUS_COMMON_PEER_CREDENTIALS_HPP
