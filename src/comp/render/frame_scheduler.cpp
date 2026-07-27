#include "comp/render/frame_scheduler.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

class DefaultSoftwareFrameSource : public IFrameSource {
public:
    FrameSourceType type() const noexcept override {
        return FrameSourceType::SoftwareTimer;
    }

    void start(std::function<void()> frame_callback) override {
        m_callback = std::move(frame_callback);
        m_active = true;
    }

    void stop() override {
        m_active = false;
    }

private:
    std::function<void()> m_callback;
    bool m_active{false};
};

FrameScheduler& FrameScheduler::instance() noexcept {
    static FrameScheduler s_instance;
    return s_instance;
}

FrameScheduler::FrameScheduler()
    : m_last_frame_time(std::chrono::steady_clock::now()),
      m_frame_source(std::make_unique<DefaultSoftwareFrameSource>()) {}

void FrameScheduler::set_target_refresh_rate(uint32_t hz) {
    m_target_hz = hz;
    m_stats.current_fps = hz;
    log::info("FrameScheduler: Target refresh rate set to {} Hz", hz);
}

uint32_t FrameScheduler::target_refresh_rate() const noexcept {
    return m_target_hz;
}

void FrameScheduler::set_frame_source(std::unique_ptr<IFrameSource> source) {
    if (!source) return;
    log::info("FrameScheduler: Switched frame source to type {}", static_cast<int>(source->type()));
    m_frame_source = std::move(source);
}

FrameSourceType FrameScheduler::active_source_type() const noexcept {
    if (m_frame_source) {
        return m_frame_source->type();
    }
    return FrameSourceType::SoftwareTimer;
}

void FrameScheduler::notify_damage() {
    m_stats.damage_pending = true;
}

void FrameScheduler::on_vblank() {
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double, std::milli> elapsed = now - m_last_frame_time;
    m_last_frame_time = now;

    m_stats.render_time_ms = elapsed.count();
    m_stats.frame_count++;
    m_stats.damage_pending = false;
}

FrameStats FrameScheduler::stats() const noexcept {
    return m_stats;
}

} // namespace tinexus::comp
