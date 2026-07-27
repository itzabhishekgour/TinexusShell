#ifndef TINEXUS_LIVEUSB_PERSISTENCE_MGR_HPP
#define TINEXUS_LIVEUSB_PERSISTENCE_MGR_HPP

#include <string>
#include <cstdint>

namespace tinexus::liveusb {

class PersistenceManager {
public:
    static bool create_persistence_volume(const std::string& device_path, uint64_t size_mb = 4096, bool dry_run = false);
};

} // namespace tinexus::liveusb

#endif // TINEXUS_LIVEUSB_PERSISTENCE_MGR_HPP
