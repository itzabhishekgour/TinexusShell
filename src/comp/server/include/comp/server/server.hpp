#ifndef TINEXUS_COMP_SERVER_HPP
#define TINEXUS_COMP_SERVER_HPP

#include "comp/server/global_registry.hpp"
#include "comp/server/surface_tree.hpp"
#include "comp/backend/backend.hpp"
#include <string>
#include <memory>
#include <chrono>

struct PendingIconQuery {
    bool     active{false};
    uint64_t surface_id{0};
    std::string app_id;
    std::chrono::steady_clock::time_point deadline;
};

struct wl_display;
struct wl_event_loop;

namespace tinexus::comp {

class TinexusServer {
public:
    static TinexusServer* instance();

    TinexusServer();
    ~TinexusServer();

    bool initialize();
    bool run();
    void stop();
    
    void trigger_minimize(uint64_t surface_id);

    [[nodiscard]] const std::string& wayland_display() const noexcept;
    [[nodiscard]] struct wl_display* display_handle() const noexcept { return m_wl_display; }
    [[nodiscard]] GlobalRegistry& registry() noexcept { return m_registry; }
    [[nodiscard]] SurfaceTree& surface_tree() noexcept { return m_surface_tree; }
    [[nodiscard]] Backend* backend() const noexcept { return m_backend.get(); }

private:
    struct wl_display* m_wl_display{nullptr};
    struct wl_event_loop* m_wl_loop{nullptr};
    std::string m_display_socket{"wayland-0"};
    bool m_running{false};
    
    std::unique_ptr<Backend> m_backend;
    
    GlobalRegistry m_registry;
    SurfaceTree m_surface_tree;

    int m_ipc_socket{-1};
    static int handle_ipc_fd(int fd, uint32_t mask, void* data);
    void setup_ipc_connection();
    void process_ipc_message(uint16_t msg_type, const void* payload, uint32_t payload_len);
    
public:
    PendingIconQuery m_pending_icon_query;
    void check_icon_query_timeout();
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SERVER_HPP
