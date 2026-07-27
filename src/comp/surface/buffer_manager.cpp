#include "comp/surface/buffer_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

bool BufferManager::attach_shm_buffer(SurfaceState& surface, void* shm_data, uint32_t width, uint32_t height, uint32_t stride) {
    auto buf = std::make_shared<BufferState>();
    buf->data = shm_data;
    buf->width = width;
    buf->height = height;
    buf->stride = stride;
    surface.pending_buffer = buf;

    log::info("BufferManager: Attached SHM buffer ({}x{}, stride {}) to pending state for WindowID #{}", width, height, stride, surface.id);
    return true;
}

void BufferManager::add_damage(SurfaceState& surface, int32_t x, int32_t y, uint32_t width, uint32_t height) {
    (void)x; (void)y;
    log::info("BufferManager: Recorded surface damage rect ({}x{}) for WindowID #{}", width, height, surface.id);
}

bool BufferManager::commit(SurfaceState& surface) {
    surface.commit_pending();
    return true;
}

} // namespace tinexus::comp
