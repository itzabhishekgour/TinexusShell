#include "common/peer_credentials.hpp"

#if defined(__linux__)
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace tinexus::security {

Result<PeerCredentials, SecurityError> get_peer_credentials(int socket_fd) noexcept {
    if (socket_fd < 0) {
        return Unexpected(SecurityError::InvalidSocket);
    }

#if defined(__linux__)
    struct ucred creds{};
    socklen_t len = sizeof(creds);
    if (getsockopt(socket_fd, SOL_SOCKET, SO_PEERCRED, &creds, &len) != 0) {
        return Unexpected(SecurityError::SocketOptionFailed);
    }
    return PeerCredentials{
        .pid = creds.pid,
        .uid = creds.uid,
        .gid = creds.gid
    };
#else
    // Fallback for non-Linux mock testing
    return PeerCredentials{
        .pid = getpid(),
        .uid = getuid(),
        .gid = getgid()
    };
#endif
}

bool verify_same_user(int socket_fd) noexcept {
    auto res = get_peer_credentials(socket_fd);
    if (!res) {
        return false;
    }
    return res.value().uid == getuid();
}

} // namespace tinexus::security
