#include "comp/render/frame_scheduler.hpp"
#include "common/logger.hpp"

namespace tinexus::comp {

FrameScheduler& FrameScheduler::instance() noexcept {
    static FrameScheduler s_instance;
    return s_instance;
}

FrameScheduler::FrameScheduler()
    : m_last_frame_time(std::chrono::steady_clock::now()) {}

void FrameScheduler::set_target_refresh_rate(uint32_t hz) {
    m_target_hz = (hz > 0) ? hz : 60;
    m_stats.current_fps = m_target_hz;
    log::info("FrameScheduler: Target refresh rate set to {} Hz", m_target_hz);
}

uint32_t FrameScheduler::target_refresh_rate() const noexcept {
    return m_target_hz;
}

void FrameScheduler::notify_damage() {
    m_stats.damage_pending = true;
}

void FrameScheduler::on_vblank() {
    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration_cast<std::chrono::microseconds>(now - m_last_frame_time);
    m_last_frame_time = now;

    if (delta.count() > 0) {
        m_stats.render_time_ms = static_cast<double>(delta.count()) / 1000.0;
    }

    m_stats.frame_count++;
    m_stats.damage_pending = false;
}

FrameStats FrameScheduler::stats() const noexcept {
    return m_stats;
}

} // namespace tinexus::comp
