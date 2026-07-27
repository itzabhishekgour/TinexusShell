#ifndef TINEXUS_PKG_PACKAGE_MANIFEST_HPP
#define TINEXUS_PKG_PACKAGE_MANIFEST_HPP

#include <string>
#include <vector>

namespace tinexus::pkg {

struct PackageManifest {
    std::string name;
    std::string version;
    std::string architecture{"x86_64"};
    std::vector<std::string> dependencies;
    std::vector<std::string> files;
    std::string sha256_checksum;
    std::string ed25519_signature;

    [[nodiscard]] bool is_valid() const noexcept {
        return !name.empty() && !version.empty() && !sha256_checksum.empty();
    }
};

class ManifestParser {
public:
    static PackageManifest parse_string(const std::string& manifest_content);
};

} // namespace tinexus::pkg

#endif // TINEXUS_PKG_PACKAGE_MANIFEST_HPP
