#include "common/logger.hpp"
#include "common/version.hpp"
#include "ipcd/transport/unix_socket.hpp"
#include "ipcd/transport/epoll_loop.hpp"
#include "ipcd/security/peer_validator.hpp"
#include "ipcd/registry/service_registry.hpp"
#include "ipcd/broker/pubsub_broker.hpp"
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pwd.h>

int main(int argc, char** argv) {
    tinexus::log::set_component_name("tinexus-ipcd");
    tinexus::log::info("Starting tinexus-ipcd v{} - IPC Broker & Service Registry", tinexus::VERSION_STRING);

    uid_t uid = getuid();
    std::string socket_path = "/run/user/" + std::to_string(uid) + "/tinexus/ipc.sock";

    tinexus::ipcd::transport::UnixSocketServer server(socket_path);
    if (!server.bind_and_listen()) {
        tinexus::log::error("Failed to start IPC server");
        return 1;
    }

    tinexus::ipcd::transport::EpollLoop loop;
    
    // Register the server socket to accept incoming connections
    if (!loop.register_fd(server.get_fd(), EPOLLIN, (void*)(intptr_t)server.get_fd())) {
        return 1;
    }

    tinexus::log::info("IPCD Event Loop running. Waiting for connections...");

    loop.run([&server, &loop](const epoll_event& ev) {
        int fd = (int)(intptr_t)ev.data.ptr;

        if (fd == server.get_fd()) {
            // Accept new connection
            int client_fd = accept4(server.get_fd(), nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (client_fd != -1) {
                // Validate peer credentials
                auto peer = tinexus::ipcd::security::PeerValidator::get_peer_identity(client_fd);
                if (peer && tinexus::ipcd::security::PeerValidator::is_authorized(*peer)) {
                    tinexus::log::debug("Accepted connection from PID: {}", peer->pid);
                    loop.register_fd(client_fd, EPOLLIN | EPOLLRDHUP, (void*)(intptr_t)client_fd);
                } else {
                    close(client_fd);
                }
            }
        } else {
            // Handle client data or disconnect
            if (ev.events & EPOLLRDHUP || ev.events & EPOLLHUP || ev.events & EPOLLERR) {
                tinexus::log::debug("Client disconnected: FD {}", fd);
                tinexus::ipcd::registry::ServiceRegistry::instance().unregister_by_fd(fd);
                tinexus::ipcd::broker::PubSubBroker::instance().unsubscribe_all(fd);
                loop.unregister_fd(fd);
                close(fd);
            } else if (ev.events & EPOLLIN) {
                // Here we would read the protocol::Header, route the message,
                // and forward it to subscribers or the registered service.
                // For now, this is just scaffolding.
                char buffer[1024];
                ssize_t bytes = read(fd, buffer, sizeof(buffer));
                if (bytes <= 0) {
                    close(fd);
                }
            }
        }
    });

    return 0;
}
