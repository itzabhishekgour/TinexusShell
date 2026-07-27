#include "comp/surface/frame_callback.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

void FrameCallbackManager::request_callback(uint64_t surface_id, uint32_t callback_id) {
    m_callbacks.push_back({surface_id, callback_id});
    log::info("FrameCallbackManager: Queued frame callback #{} for WindowID #{}", callback_id, surface_id);
}

size_t FrameCallbackManager::send_frame_done_all(uint32_t timestamp_ms) {
    size_t count = m_callbacks.size();
    if (count > 0) {
        log::info("FrameCallbackManager: Dispatched wl_callback_send_done (ts: {} ms) to {} pending frame callbacks", timestamp_ms, count);
        m_callbacks.clear();
    }
    return count;
}

size_t FrameCallbackManager::pending_callbacks_count() const noexcept {
    return m_callbacks.size();
}

} // namespace tinexus::comp
