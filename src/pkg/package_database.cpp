#include "pkg/package_database.hpp"
#include "common/logger.hpp"

namespace tinexus::pkg {

bool PackageDatabase::register_package(const PackageManifest& manifest) {
    m_db[manifest.name] = manifest;
    log::info("PackageDatabase: Registered package '{}-{}' in database", manifest.name, manifest.version);
    return true;
}

bool PackageDatabase::unregister_package(const std::string& name) {
    if (m_db.erase(name) > 0) {
        log::info("PackageDatabase: Unregistered package '{}' from database", name);
        return true;
    }
    return false;
}

bool PackageDatabase::is_installed(const std::string& name) const {
    return m_db.find(name) != m_db.end();
}

std::vector<PackageManifest> PackageDatabase::list_installed() const {
    std::vector<PackageManifest> list;
    for (const auto& [name, manifest] : m_db) {
        list.push_back(manifest);
    }
    return list;
}

} // namespace tinexus::pkg
