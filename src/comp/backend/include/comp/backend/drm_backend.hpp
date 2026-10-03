#ifndef TINEXUS_COMP_BACKEND_DRM_BACKEND_HPP
#define TINEXUS_COMP_BACKEND_DRM_BACKEND_HPP

#include "comp/backend/backend.hpp"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace tinexus::comp {

enum class GpuVendor {
    Intel,
    Amd,
    Nvidia,
    Unknown
};

struct DrmConnectorInfo {
    uint32_t connector_id{0};
    std::string name;
    bool connected{false};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t refresh_hz{0};
};

struct GbmBuffer {
    uint32_t fb_id{0};
    uint32_t width{0};
    uint32_t height{0};
    uint32_t stride{0};
    std::vector<uint32_t> pixels; // ARGB8888
};

class DrmBackend {
public:
    explicit DrmBackend(std::string device_path = "");
    ~DrmBackend() = default;

    bool initialize();
    void poll_events();
    void swap_buffers();

    [[nodiscard]] std::string type_name() const noexcept { return "DrmBackend"; }
    [[nodiscard]] bool is_atomic_supported() const noexcept { return m_atomic_supported; }
    [[nodiscard]] GpuVendor gpu_vendor() const noexcept { return m_gpu_vendor; }
    [[nodiscard]] const std::vector<DrmConnectorInfo>& connectors() const noexcept { return m_connectors; }

    [[nodiscard]] GbmBuffer allocate_gbm_buffer(uint32_t width, uint32_t height);
    bool commit_atomic_page_flip(const GbmBuffer& buf);

private:
    std::string m_device_path;
    int m_fd{-1};
    bool m_atomic_supported{true};
    bool m_initialized{false};
    GpuVendor m_gpu_vendor{GpuVendor::Intel};
    std::vector<DrmConnectorInfo> m_connectors;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_BACKEND_DRM_BACKEND_HPP
