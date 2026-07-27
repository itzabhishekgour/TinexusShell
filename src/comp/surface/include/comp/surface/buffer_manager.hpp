#ifndef TINEXUS_COMP_BUFFER_MANAGER_HPP
#define TINEXUS_COMP_BUFFER_MANAGER_HPP

#include "comp/surface/surface_state.hpp"
#include <memory>

namespace tinexus::comp {

class BufferManager {
public:
    BufferManager() = default;
    ~BufferManager() = default;

    bool attach_shm_buffer(SurfaceState& surface, void* shm_data, uint32_t width, uint32_t height, uint32_t stride);
    void add_damage(SurfaceState& surface, int32_t x, int32_t y, uint32_t width, uint32_t height);
    bool commit(SurfaceState& surface);
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_BUFFER_MANAGER_HPP
