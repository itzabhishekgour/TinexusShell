#include "pkg/package_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::pkg {

bool PackageManager::install_package(const PackageManifest& manifest) {
    log::info("PackageManager: Executing install command for '{}-{}'", manifest.name, manifest.version);
    return m_tx.begin_install(manifest, m_db);
}

bool PackageManager::remove_package(const std::string& name) {
    log::info("PackageManager: Executing remove command for package '{}'", name);
    return m_db.unregister_package(name);
}

bool PackageManager::is_installed(const std::string& name) const {
    return m_db.is_installed(name);
}

} // namespace tinexus::pkg
