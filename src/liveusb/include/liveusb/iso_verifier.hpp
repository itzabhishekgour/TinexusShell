#ifndef TINEXUS_LIVEUSB_ISO_VERIFIER_HPP
#define TINEXUS_LIVEUSB_ISO_VERIFIER_HPP

#include <string>

namespace tinexus::liveusb {

class IsoVerifier {
public:
    static bool verify_iso_integrity(const std::string& iso_path);
};

} // namespace tinexus::liveusb

#endif // TINEXUS_LIVEUSB_ISO_VERIFIER_HPP
