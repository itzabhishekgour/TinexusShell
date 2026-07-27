#ifndef TINEXUS_COMP_RENDERER_GPU_RESOURCE_MANAGER_HPP
#define TINEXUS_COMP_RENDERER_GPU_RESOURCE_MANAGER_HPP

#include <cstdint>
#include <map>
#include <string>

namespace tinexus::comp {

struct GpuResourceHandle {
    uint64_t id{0};
    std::string name;
    size_t size_bytes{0};
};

class GpuResourceManager {
public:
    static GpuResourceManager& instance() noexcept;

    GpuResourceManager() = default;
    ~GpuResourceManager() = default;

    GpuResourceHandle create_texture(const std::string& name, uint32_t width, uint32_t height);
    bool destroy_texture(uint64_t id);

    GpuResourceHandle create_pipeline(const std::string& shader_name);
    bool destroy_pipeline(uint64_t id);

    [[nodiscard]] size_t total_gpu_memory_allocated() const noexcept { return m_allocated_memory; }
    [[nodiscard]] size_t active_resource_count() const noexcept { return m_resources.size(); }

private:
    uint64_t m_next_id{1001};
    size_t m_allocated_memory{0};
    std::map<uint64_t, GpuResourceHandle> m_resources;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_RENDERER_GPU_RESOURCE_MANAGER_HPP
