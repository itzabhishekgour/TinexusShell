#include "comp/renderer/gpu_resource_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

GpuResourceManager& GpuResourceManager::instance() noexcept {
    static GpuResourceManager s_instance;
    return s_instance;
}

GpuResourceHandle GpuResourceManager::create_texture(const std::string& name, uint32_t width, uint32_t height) {
    size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    uint64_t id = m_next_id++;
    GpuResourceHandle handle{id, name, bytes};
    m_resources[id] = handle;
    m_allocated_memory += bytes;
    log::info("GpuResourceManager: Created GPU Texture '{}' ID={} ({}x{}, {} bytes)", name, id, width, height, bytes);
    return handle;
}

bool GpuResourceManager::destroy_texture(uint64_t id) {
    auto it = m_resources.find(id);
    if (it == m_resources.end()) return false;
    m_allocated_memory -= it->second.size_bytes;
    log::info("GpuResourceManager: Destroyed GPU Texture ID={}", id);
    m_resources.erase(it);
    return true;
}

GpuResourceHandle GpuResourceManager::create_pipeline(const std::string& shader_name) {
    uint64_t id = m_next_id++;
    size_t pipeline_size = 4096;
    GpuResourceHandle handle{id, shader_name, pipeline_size};
    m_resources[id] = handle;
    m_allocated_memory += pipeline_size;
    log::info("GpuResourceManager: Compiled & Bound GPU Pipeline '{}' ID={}", shader_name, id);
    return handle;
}

bool GpuResourceManager::destroy_pipeline(uint64_t id) {
    return destroy_texture(id);
}

} // namespace tinexus::comp
