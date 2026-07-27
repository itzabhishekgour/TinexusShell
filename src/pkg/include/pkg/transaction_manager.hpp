#ifndef TINEXUS_PKG_TRANSACTION_MANAGER_HPP
#define TINEXUS_PKG_TRANSACTION_MANAGER_HPP

#include "pkg/package_manifest.hpp"
#include "pkg/package_database.hpp"

namespace tinexus::pkg {

enum class TransactionState {
    Idle,
    Verifying,
    Resolving,
    Staging,
    Committed,
    RolledBack
};

class TransactionManager {
public:
    TransactionManager() = default;
    ~TransactionManager() = default;

    bool begin_install(const PackageManifest& manifest, PackageDatabase& db);
    bool rollback();

    [[nodiscard]] TransactionState state() const noexcept { return m_state; }

private:
    TransactionState m_state{TransactionState::Idle};
    PackageManifest m_staged_manifest;
};

} // namespace tinexus::pkg

#endif // TINEXUS_PKG_TRANSACTION_MANAGER_HPP
