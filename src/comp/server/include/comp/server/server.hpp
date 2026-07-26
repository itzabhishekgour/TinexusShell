#ifndef TINEXUS_COMP_SERVER_HPP
#define TINEXUS_COMP_SERVER_HPP

#include <string>

namespace tinexus::comp {

class TinexusServer {
public:
    TinexusServer();
    ~TinexusServer();

    bool initialize();
    void run();
    void stop();

    [[nodiscard]] const std::string& wayland_display() const noexcept;

private:
    std::string m_display_socket{"wayland-0"};
    bool m_running{false};
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SERVER_HPP
