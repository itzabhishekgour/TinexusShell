#include "comp/surface/surface_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

SurfaceManager& SurfaceManager::instance() noexcept {
    static SurfaceManager s_instance;
    return s_instance;
}

uint32_t SurfaceManager::create_surface(pid_t pid, const std::string& app_id) {
    uint32_t id = m_next_id++;
    SurfaceRecord record{id, pid, app_id, 1, "HDMI-A-1", SurfaceState::Created};
    m_surfaces[id] = record;

    log::info("PID={} APP={} SURFACE={} WORKSPACE={} OUTPUT={} STATE={}",
              pid, app_id, id, record.workspace_id, record.output_name, surface_state_to_string(record.state));

    return id;
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
