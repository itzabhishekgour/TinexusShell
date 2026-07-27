#include "liveusb/progress_tracker.hpp"
#include "common/logger.hpp"

namespace tinexus::liveusb {

void ProgressTracker::update_progress(uint64_t bytes_written, uint64_t total_bytes) {
    if (total_bytes > 0) {
        m_percent = (static_cast<double>(bytes_written) / static_cast<double>(total_bytes)) * 100.0;
    }
    log::info("ProgressTracker: {:.1f}% completed (Written: {} / {} bytes @ {:.1f} MB/s)", m_percent, bytes_written, total_bytes, m_speed_mbps);
}

} // namespace tinexus::liveusb
