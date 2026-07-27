#ifndef TINEXUS_LIVEUSB_RAW_WRITER_HPP
#define TINEXUS_LIVEUSB_RAW_WRITER_HPP

#include <string>

namespace tinexus::liveusb {

class RawWriter {
public:
    static bool write_image(const std::string& iso_path, const std::string& device_path, bool dry_run = false);
};

} // namespace tinexus::liveusb

#endif // TINEXUS_LIVEUSB_RAW_WRITER_HPP
