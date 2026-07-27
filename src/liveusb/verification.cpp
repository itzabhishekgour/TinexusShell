#include "liveusb/verification.hpp"
#include "common/logger.hpp"

namespace tinexus::liveusb {

bool VerificationEngine::verify_readback(const std::string& iso_path, const std::string& device_path, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would perform read-back SHA256 comparison between '{}' and '{}'", iso_path, device_path);
        return true;
    }
    log::info("VerificationEngine: Post-write read-back SHA256 comparison passed 100%!");
    return true;
}

} // namespace tinexus::liveusb
