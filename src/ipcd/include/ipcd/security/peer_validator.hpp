#ifndef TINEXUS_IPCD_SECURITY_PEER_VALIDATOR_HPP
#define TINEXUS_IPCD_SECURITY_PEER_VALIDATOR_HPP

#include <sys/socket.h>
#include <unistd.h>
#include <string>
#include <optional>
#include <fstream>
#include "common/logger.hpp"

namespace tinexus::ipcd::security {

struct PeerIdentity {
    pid_t pid;
    uid_t uid;
    gid_t gid;
    std::string executable_path;
    // std::string sha256_hash; // Future plugin security
};

class PeerValidator {
public:
    static std::optional<PeerIdentity> get_peer_identity(int client_fd) {
#ifdef __linux__
        struct ucred creds;
        socklen_t len = sizeof(creds);
        if (getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &creds, &len) == 0) {
            PeerIdentity id;
            id.pid = creds.pid;
            id.uid = creds.uid;
            id.gid = creds.gid;
            
            // Resolve executable path
            char exe_path[1024];
            std::string proc_path = "/proc/" + std::to_string(id.pid) + "/exe";
            ssize_t path_len = readlink(proc_path.c_str(), exe_path, sizeof(exe_path) - 1);
            if (path_len > 0) {
                exe_path[path_len] = '\0';
                id.executable_path = std::string(exe_path);
            } else {
                id.executable_path = "unknown";
            }
            return id;
        }
#endif
        tinexus::log::error("Failed to get peer credentials on socket fd: {}", client_fd);
        return std::nullopt;
    }

    static bool is_authorized(const PeerIdentity& id) {
        // Enforce same user UID
        if (id.uid != getuid()) {
            tinexus::log::warn("Rejected connection from cross-user UID: {}", id.uid);
            return false;
        }

        // Future whitelist validation by executable_path or SHA256 could go here
        return true;
    }
};

} // namespace tinexus::ipcd::security

#endif // TINEXUS_IPCD_SECURITY_PEER_VALIDATOR_HPP
