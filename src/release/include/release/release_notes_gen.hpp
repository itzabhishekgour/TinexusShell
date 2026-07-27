#ifndef TINEXUS_RELEASE_RELEASE_NOTES_GEN_HPP
#define TINEXUS_RELEASE_RELEASE_NOTES_GEN_HPP

#include <string>

namespace tinexus::release {

class ReleaseNotesGen {
public:
    static bool generate_release_notes(const std::string& version, const std::string& output_md, bool dry_run = false);
};

} // namespace tinexus::release

#endif // TINEXUS_RELEASE_RELEASE_NOTES_GEN_HPP
