#ifndef TINEXUS_COMP_XDG_SHELL_MANAGER_HPP
#define TINEXUS_COMP_XDG_SHELL_MANAGER_HPP

#include "comp/surface/surface_state.hpp"
#include "comp/surface/configure_serial.hpp"
#include <memory>
#include <unordered_map>

namespace tinexus::comp {

class XdgShellManager {
public:
    XdgShellManager() = default;
    ~XdgShellManager() = default;

    bool bind_xdg_wm_base(uint32_t version);
    uint64_t create_xdg_surface(uint64_t surface_id);
    uint32_t send_toplevel_configure(uint64_t surface_id, int32_t width, int32_t height);
    bool handle_ack_configure(uint64_t surface_id, uint32_t serial);

    [[nodiscard]] size_t active_xdg_surfaces_count() const noexcept { return m_xdg_surfaces.size(); }

private:
    ConfigureSerialManager m_serial_mgr;
    std::unordered_map<uint64_t, SurfaceState> m_xdg_surfaces;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_XDG_SHELL_MANAGER_HPP
