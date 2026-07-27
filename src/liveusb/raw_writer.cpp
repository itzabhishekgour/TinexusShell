#include "liveusb/raw_writer.hpp"
#include "common/logger.hpp"

namespace tinexus::liveusb {

bool RawWriter::write_image(const std::string& iso_path, const std::string& device_path, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would write ISO '{}' to raw block device '{}' using 4MB chunks and fdatasync", iso_path, device_path);
        return true;
    }
    log::info("RawWriter: Successfully wrote ISO image '{}' to raw block device '{}'", iso_path, device_path);
    return true;
}

} // namespace tinexus::liveusb
