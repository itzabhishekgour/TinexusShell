#include "comp/surface/surface_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

SurfaceManager& SurfaceManager::instance() noexcept {
    static SurfaceManager s_instance;
    return s_instance;
}

uint32_t SurfaceManager::create_surface(pid_t pid, const std::string& app_id) {
    uint32_t id = m_next_id++;
    SurfaceRecord record{id, pid, app_id, 1, "", SurfaceState::Created, SurfaceRole::None, SurfaceLifecycle::Created};
    m_surfaces[id] = record;

    log::info("PID={} APP={} SURFACE={} WORKSPACE={} OUTPUT={} STATE={} LIFECYCLE={}",
              pid, app_id, id, record.workspace_id, record.output_name, surface_state_to_string(record.state), surface_lifecycle_to_string(record.lifecycle));

    return id;
}

bool SurfaceManager::remove_surface(uint32_t surface_id) {
    auto it = m_surfaces.find(surface_id);
    if (it == m_surfaces.end()) return false;

    log::info("SURFACE={} LIFECYCLE=Destroyed (removed from SurfaceManager)", surface_id);
    m_surfaces.erase(it);
    return true;
}

bool SurfaceManager::assign_role(uint32_t surface_id, SurfaceRole role) {
    auto it = m_surfaces.find(surface_id);
    if (it == m_surfaces.end()) return false;

    auto& record = it->second;
    if (record.role != SurfaceRole::None && record.role != role) {
        log::error("SURFACE={} cannot reassign role from {} to {}", surface_id, surface_role_to_string(record.role), surface_role_to_string(role));
        return false;
    }

    record.role = role;
    if (record.lifecycle == SurfaceLifecycle::Created) {
        record.lifecycle = SurfaceLifecycle::RoleAssigned;
    }

    log::info("SURFACE={} ROLE={} LIFECYCLE={}", surface_id, surface_role_to_string(record.role), surface_lifecycle_to_string(record.lifecycle));
    return true;
}

bool SurfaceManager::transition_lifecycle(uint32_t surface_id, SurfaceLifecycle new_lifecycle) {
    auto it = m_surfaces.find(surface_id);
    if (it == m_surfaces.end()) return false;

    auto& record = it->second;
    // Cannot transition beyond Created without an assigned role unless being destroyed
    if (record.role == SurfaceRole::None && new_lifecycle != SurfaceLifecycle::Destroyed && new_lifecycle != SurfaceLifecycle::Created) {
        log::error("SURFACE={} cannot transition to {} without assigned role", surface_id, surface_lifecycle_to_string(new_lifecycle));
        return false;
    }

    record.lifecycle = new_lifecycle;

    log::info("SURFACE={} ROLE={} LIFECYCLE={}", surface_id, surface_role_to_string(record.role), surface_lifecycle_to_string(record.lifecycle));
    return true;
}

bool SurfaceManager::transition_state(uint32_t surface_id, SurfaceState new_state) {
    auto it = m_surfaces.find(surface_id);
    if (it == m_surfaces.end()) return false;

    auto& record = it->second;
    record.state = new_state;

    log::info("PID={} APP={} SURFACE={} WORKSPACE={} OUTPUT={} STATE={}",
              record.pid, record.app_id, record.surface_id, record.workspace_id, record.output_name, surface_state_to_string(record.state));

    return true;
}

SurfaceRecord SurfaceManager::get_record(uint32_t surface_id) const {
    auto it = m_surfaces.find(surface_id);
    if (it != m_surfaces.end()) return it->second;
    return SurfaceRecord{};
}

std::vector<SurfaceRecord> SurfaceManager::get_all_surfaces() const {
    std::vector<SurfaceRecord> result;
    for (const auto& [id, record] : m_surfaces) {
        result.push_back(record);
    }
    return result;
}

} // namespace tinexus::comp
