#ifndef TINEXUS_RELEASE_RELEASE_MANIFEST_HPP
#define TINEXUS_RELEASE_RELEASE_MANIFEST_HPP

#include <string>

namespace tinexus::release {

class ReleaseManifest {
public:
    static bool generate_manifest_json(const std::string& version, const std::string& output_json, bool dry_run = false);
};

} // namespace tinexus::release

#endif // TINEXUS_RELEASE_RELEASE_MANIFEST_HPP
