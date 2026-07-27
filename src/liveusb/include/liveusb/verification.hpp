#ifndef TINEXUS_LIVEUSB_VERIFICATION_HPP
#define TINEXUS_LIVEUSB_VERIFICATION_HPP

#include <string>

namespace tinexus::liveusb {

class VerificationEngine {
public:
    static bool verify_readback(const std::string& iso_path, const std::string& device_path, bool dry_run = false);
};

} // namespace tinexus::liveusb

#endif // TINEXUS_LIVEUSB_VERIFICATION_HPP
