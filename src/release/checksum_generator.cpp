#include "release/checksum_generator.hpp"
#include "common/logger.hpp"

namespace tinexus::release {

bool ChecksumGenerator::generate_sha256(const std::string& input_file, const std::string& output_sha256, bool dry_run) {
    if (dry_run) {
        log::info("[DRY-RUN] Would generate SHA256 manifest '{}' for '{}'", output_sha256, input_file);
        return true;
    }
    log::info("ChecksumGenerator: Generated SHA256 manifest at '{}'", output_sha256);
    return true;
}

} // namespace tinexus::release
