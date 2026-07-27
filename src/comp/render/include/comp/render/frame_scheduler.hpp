#ifndef TINEXUS_COMP_FRAME_SCHEDULER_HPP
#define TINEXUS_COMP_FRAME_SCHEDULER_HPP

#include <cstdint>
#include <chrono>
#include <memory>
#include <functional>

namespace tinexus::comp {

enum class FrameSourceType {
    SoftwareTimer,
    VBlank,
    DrmPageFlip,
    VirtualOutput
};

class IFrameSource {
public:
    virtual ~IFrameSource() = default;

    [[nodiscard]] virtual FrameSourceType type() const noexcept = 0;
    virtual void start(std::function<void()> frame_callback) = 0;
    virtual void stop() = 0;
};

struct FrameStats {
    uint32_t current_fps{60};
    uint64_t frame_count{0};
    double render_time_ms{0.0};
    bool damage_pending{false};
};

class FrameScheduler {
public:
    static FrameScheduler& instance() noexcept;

    FrameScheduler();
    ~FrameScheduler() = default;

    void set_target_refresh_rate(uint32_t hz);
    [[nodiscard]] uint32_t target_refresh_rate() const noexcept;

    void set_frame_source(std::unique_ptr<IFrameSource> source);
    [[nodiscard]] FrameSourceType active_source_type() const noexcept;

    void notify_damage();
    void on_vblank();

    [[nodiscard]] FrameStats stats() const noexcept;

private:
    uint32_t m_target_hz{60};
    FrameStats m_stats;
    std::chrono::steady_clock::time_point m_last_frame_time;
    std::unique_ptr<IFrameSource> m_frame_source;
};

} // namespace tinexus::comp

#endif // TINEXUS_COMP_FRAME_SCHEDULER_HPP
