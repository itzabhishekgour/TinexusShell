#ifndef TINEXUS_RELEASE_ARTIFACT_PACKAGER_HPP
#define TINEXUS_RELEASE_ARTIFACT_PACKAGER_HPP

#include <string>

namespace tinexus::release {

class ArtifactPackager {
public:
    static bool package_release_artifacts(const std::string& release_dir, bool dry_run = false);
};

} // namespace tinexus::release

#endif // TINEXUS_RELEASE_ARTIFACT_PACKAGER_HPP
