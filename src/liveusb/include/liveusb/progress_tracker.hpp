#ifndef TINEXUS_LIVEUSB_PROGRESS_TRACKER_HPP
#define TINEXUS_LIVEUSB_PROGRESS_TRACKER_HPP

#include <cstdint>
#include <string>

namespace tinexus::liveusb {

class ProgressTracker {
public:
    ProgressTracker() = default;
    ~ProgressTracker() = default;

    void update_progress(uint64_t bytes_written, uint64_t total_bytes);
    [[nodiscard]] double current_speed_mbps() const noexcept { return m_speed_mbps; }
    [[nodiscard]] double percentage() const noexcept { return m_percent; }

private:
    double m_speed_mbps{42.0};
    double m_percent{0.0};
};

} // namespace tinexus::liveusb

#endif // TINEXUS_LIVEUSB_PROGRESS_TRACKER_HPP
