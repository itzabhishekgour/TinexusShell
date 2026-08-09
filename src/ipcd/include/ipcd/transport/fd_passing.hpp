#ifndef TINEXUS_IPCD_TRANSPORT_FD_PASSING_HPP
#define TINEXUS_IPCD_TRANSPORT_FD_PASSING_HPP

#include <sys/socket.h>
#include <vector>
#include <cstdint>
#include <unistd.h>
#include <cstddef>
#include "common/logger.hpp"

namespace tinexus::ipcd::transport {

// Reads data and optionally file descriptors from a Unix domain socket using recvmsg.
// Appends read data to `data_out` and received FDs to `fds_out`.
// Returns the number of bytes read, 0 on EOF, or -1 on error (with errno set).
inline ssize_t recvmsg_with_fds(int fd, std::vector<uint8_t>& data_out, std::vector<int>& fds_out) {
    uint8_t buffer[4096];
    struct iovec iov;
    iov.iov_base = buffer;
    iov.iov_len = sizeof(buffer);

    union {
        struct cmsghdr cmh;
        char control[CMSG_SPACE(sizeof(int) * 16)];
    } control_un;

    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_un.control;
    msg.msg_controllen = sizeof(control_un.control);

    ssize_t n = recvmsg(fd, &msg, MSG_DONTWAIT);
    if (n <= 0) return n;

    data_out.insert(data_out.end(), buffer, buffer + n);

    if (msg.msg_controllen > 0) {
        for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                size_t fd_count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                int* passed_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
                for (size_t i = 0; i < fd_count; ++i) {
                    fds_out.push_back(passed_fds[i]);
                }
            }
        }
    }

    return n;
}

// Sends a buffer along with an array of file descriptors using sendmsg.
// The fds are sent only on the first successful transmission to avoid duplication if partial send occurs.
// Returns the number of bytes written, or -1 on error.
inline ssize_t sendmsg_with_fds(int fd, const void* data, size_t len, const std::vector<int>& fds) {
    struct iovec iov;
    iov.iov_base = const_cast<void*>(data);
    iov.iov_len = len;

    union {
        struct cmsghdr cmh;
        char control[CMSG_SPACE(sizeof(int) * 16)];
    } control_un;

    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (!fds.empty() && fds.size() <= 16) {
        msg.msg_control = control_un.control;
        msg.msg_controllen = CMSG_SPACE(sizeof(int) * fds.size());
        
        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * fds.size());
        
        int* fd_ptr = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        for (size_t i = 0; i < fds.size(); ++i) {
            fd_ptr[i] = fds[i];
        }
    } else {
        msg.msg_control = nullptr;
        msg.msg_controllen = 0;
    }

    // Note: MSG_NOSIGNAL prevents SIGPIPE if the other end is closed
    return sendmsg(fd, &msg, MSG_NOSIGNAL);
}

// Verifies the peer credentials of a Unix domain socket
// Returns the uid of the peer, or -1 on error.
inline uid_t get_peer_uid(int fd) {
    struct ucred cred;
    socklen_t len = sizeof(cred);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1) {
        return static_cast<uid_t>(-1);
    }
    return cred.uid;
}

} // namespace tinexus::ipcd::transport

#endif // TINEXUS_IPCD_TRANSPORT_FD_PASSING_HPP
