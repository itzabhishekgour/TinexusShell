#ifndef TINEXUS_PKG_PACKAGE_DATABASE_HPP
#define TINEXUS_PKG_PACKAGE_DATABASE_HPP

#include "pkg/package_manifest.hpp"
#include <vector>
#include <unordered_map>

namespace tinexus::pkg {

class PackageDatabase {
public:
    PackageDatabase() = default;
    ~PackageDatabase() = default;

    bool register_package(const PackageManifest& manifest);
    bool unregister_package(const std::string& name);
    [[nodiscard]] bool is_installed(const std::string& name) const;
    [[nodiscard]] std::vector<PackageManifest> list_installed() const;

private:
    std::unordered_map<std::string, PackageManifest> m_db;
};

} // namespace tinexus::pkg

#endif // TINEXUS_PKG_PACKAGE_DATABASE_HPP
