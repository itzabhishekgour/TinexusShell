#ifndef TINEXUS_RELEASE_CHECKSUM_GENERATOR_HPP
#define TINEXUS_RELEASE_CHECKSUM_GENERATOR_HPP

#include <string>

namespace tinexus::release {

class ChecksumGenerator {
public:
    static bool generate_sha256(const std::string& input_file, const std::string& output_sha256, bool dry_run = false);
};

} // namespace tinexus::release

#endif // TINEXUS_RELEASE_CHECKSUM_GENERATOR_HPP
