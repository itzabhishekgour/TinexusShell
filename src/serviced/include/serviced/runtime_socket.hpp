#ifndef TINEXUS_SERVICED_RUNTIME_SOCKET_HPP
#define TINEXUS_SERVICED_RUNTIME_SOCKET_HPP

#include "serviced/process_manager.hpp"
#include <string>

namespace tinexus::serviced {

class RuntimeControlSocket {
public:
    explicit RuntimeControlSocket(ProcessManager& pm);
    ~RuntimeControlSocket();

    bool start();
    void run_accept_loop();
    void stop();
    void process_command(int client_fd, std::string_view cmd);

private:
    ProcessManager& m_pm;
    std::string m_socket_path;
    int m_server_fd{-1};
    bool m_running{false};
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_RUNTIME_SOCKET_HPP
