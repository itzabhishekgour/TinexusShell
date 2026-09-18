#ifndef TINEXUS_COMP_SURFACE_MANAGER_HPP
#define TINEXUS_COMP_SURFACE_MANAGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <sys/types.h>

namespace tinexus::comp {

enum class SurfaceRole : uint8_t {
    None = 0,
    XdgToplevel = 1,
    XdgPopup = 2,
    LayerSurface = 3,
    Subsurface = 4,
    Cursor = 5,
    DragIcon = 6
};

inline const char* surface_role_to_string(SurfaceRole role) noexcept {
    switch (role) {
        case SurfaceRole::None: return "None";
        case SurfaceRole::XdgToplevel: return "XdgToplevel";
        case SurfaceRole::XdgPopup: return "XdgPopup";
        case SurfaceRole::LayerSurface: return "LayerSurface";
        case SurfaceRole::Subsurface: return "Subsurface";
        case SurfaceRole::Cursor: return "Cursor";
        case SurfaceRole::DragIcon: return "DragIcon";
        default: return "Unknown";
    }
}

enum class SurfaceLifecycle : uint8_t {
    Created = 0,
    RoleAssigned = 1,
    Configured = 2,
    Mapped = 3,
    Visible = 4,
    Hidden = 5,
    Unmapped = 6,
    Destroyed = 7
};

inline const char* surface_lifecycle_to_string(SurfaceLifecycle lc) noexcept {
    switch (lc) {
        case SurfaceLifecycle::Created: return "Created";
        case SurfaceLifecycle::RoleAssigned: return "RoleAssigned";
        case SurfaceLifecycle::Configured: return "Configured";
        case SurfaceLifecycle::Mapped: return "Mapped";
        case SurfaceLifecycle::Visible: return "Visible";
        case SurfaceLifecycle::Hidden: return "Hidden";
        case SurfaceLifecycle::Unmapped: return "Unmapped";
        case SurfaceLifecycle::Destroyed: return "Destroyed";
        default: return "Unknown";
    }
}

enum class SurfaceState : uint8_t {
    Created = 0,
    Configured = 1,
    Mapped = 2,
    Focused = 3,
    Minimized = 4,
    Unmapped = 5,
    Destroyed = 6
};

inline const char* surface_state_to_string(SurfaceState state) noexcept {
    switch (state) {
        case SurfaceState::Created: return "Created";
        case SurfaceState::Configured: return "Configured";
        case SurfaceState::Mapped: return "Mapped";
        case SurfaceState::Focused: return "Focused";
        case SurfaceState::Minimized: return "Minimized";
        case SurfaceState::Unmapped: return "Unmapped";
        case SurfaceState::Destroyed: return "Destroyed";
        default: return "Unknown";
    }
}

struct SurfaceRecord {
    uint32_t surface_id{0};
    pid_t pid{-1};
    std::string app_id;
    uint32_t workspace_id{1};
    std::string output_name;
    SurfaceState state{SurfaceState::Created};
    SurfaceRole role{SurfaceRole::None};
    SurfaceLifecycle lifecycle{SurfaceLifecycle::Created};
};

class SurfaceManager {
public:
    static SurfaceManager& instance() noexcept;

    SurfaceManager() = default;
    ~SurfaceManager() = default;

    uint32_t create_surface(pid_t pid, const std::string& app_id);
    bool remove_surface(uint32_t surface_id);
    bool assign_role(uint32_t surface_id, SurfaceRole role);
    bool transition_lifecycle(uint32_t surface_id, SurfaceLifecycle new_lifecycle);
    bool transition_state(uint32_t surface_id, SurfaceState new_state);
    SurfaceRecord get_record(uint32_t surface_id) const;
    std::vector<SurfaceRecord> get_all_surfaces() const;

private:
    uint32_t m_next_id{1};
    std::unordered_map<uint32_t, SurfaceRecord> m_surfaces;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SURFACE_MANAGER_HPP
