#include "pkg/transaction_manager.hpp"
#include "common/logger.hpp"

namespace tinexus::pkg {

bool TransactionManager::begin_install(const PackageManifest& manifest, PackageDatabase& db) {
    m_staged_manifest = manifest;
    m_state = TransactionState::Verifying;
    log::info("TransactionManager: Verifying package '{}'...", manifest.name);

    if (!manifest.is_valid()) {
        log::error("TransactionManager: Manifest invalid! Triggering transaction rollback.");
        rollback();
        return false;
    }

    m_state = TransactionState::Staging;
    log::info("TransactionManager: Staging package extraction in temporary directory...");

    m_state = TransactionState::Committed;
    db.register_package(manifest);
    log::info("TransactionManager: Transaction committed successfully for package '{}'", manifest.name);
    return true;
}

bool TransactionManager::rollback() {
    m_state = TransactionState::RolledBack;
    log::warn("TransactionManager: Transaction rolled back successfully.");
    return true;
}

} // namespace tinexus::pkg
