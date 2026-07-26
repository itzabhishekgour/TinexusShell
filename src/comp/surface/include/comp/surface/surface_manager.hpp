#ifndef TINEXUS_COMP_SURFACE_MANAGER_HPP
#define TINEXUS_COMP_SURFACE_MANAGER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <sys/types.h>

namespace tinexus::comp {

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
    std::string output_name{"HDMI-A-1"};
    SurfaceState state{SurfaceState::Created};
};

class SurfaceManager {
public:
    static SurfaceManager& instance() noexcept;

    SurfaceManager() = default;
    ~SurfaceManager() = default;

    uint32_t create_surface(pid_t pid, const std::string& app_id);
    bool transition_state(uint32_t surface_id, SurfaceState new_state);
    SurfaceRecord get_record(uint32_t surface_id) const;
    std::vector<SurfaceRecord> get_all_surfaces() const;

private:
    uint32_t m_next_id{1};
    std::unordered_map<uint32_t, SurfaceRecord> m_surfaces;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_SURFACE_MANAGER_HPP
