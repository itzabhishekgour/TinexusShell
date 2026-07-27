#ifndef TINEXUS_COMP_FRAME_CALLBACK_HPP
#define TINEXUS_COMP_FRAME_CALLBACK_HPP

#include <cstdint>
#include <vector>

namespace tinexus::comp {

struct FrameCallbackRequest {
    uint64_t surface_id;
    uint32_t callback_id;
};

class FrameCallbackManager {
public:
    FrameCallbackManager() = default;
    ~FrameCallbackManager() = default;

    void request_callback(uint64_t surface_id, uint32_t callback_id);
    size_t send_frame_done_all(uint32_t timestamp_ms);
    [[nodiscard]] size_t pending_callbacks_count() const noexcept;

private:
    std::vector<FrameCallbackRequest> m_callbacks;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_FRAME_CALLBACK_HPP
