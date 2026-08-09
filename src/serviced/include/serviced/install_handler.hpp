#ifndef TINEXUS_SERVICED_INSTALL_HANDLER_HPP
#define TINEXUS_SERVICED_INSTALL_HANDLER_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <atomic>

namespace tinexus::serviced {

class InstallHandler {
public:
    static InstallHandler& instance() noexcept {
        static InstallHandler handler;
        return handler;
    }

    /**
     * @brief Processes an incoming INSTALL_REQUEST
     * @param app_name The requested application name (e.g., "com.example.app")
     * @param payload_fd The file descriptor containing the application payload
     * @param signature_bytes The 64-byte Ed25519 signature
     * @return true if installation succeeds, false otherwise
     */
    bool handle_install_request(const std::string& app_name, int payload_fd, const std::vector<uint8_t>& signature_bytes);

private:
    InstallHandler() = default;
    ~InstallHandler() = default;

    bool is_valid_app_name(const std::string& name) const;
    bool copy_fd_to_path(int fd, const std::string& target_path) const;

    std::atomic<int> m_active_installs{0};
    const int MAX_CONCURRENT_INSTALLS = 3;
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_INSTALL_HANDLER_HPP
