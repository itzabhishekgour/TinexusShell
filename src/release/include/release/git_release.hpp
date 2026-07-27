#ifndef TINEXUS_RELEASE_GIT_RELEASE_HPP
#define TINEXUS_RELEASE_GIT_RELEASE_HPP

#include <string>

namespace tinexus::release {

class GitRelease {
public:
    static bool validate_git_tag_readiness(const std::string& tag_name);
};

} // namespace tinexus::release

#endif // TINEXUS_RELEASE_GIT_RELEASE_HPP
