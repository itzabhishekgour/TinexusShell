#ifndef TINEXUS_PKG_PACKAGE_MANAGER_HPP
#define TINEXUS_PKG_PACKAGE_MANAGER_HPP

#include "pkg/package_database.hpp"
#include "pkg/transaction_manager.hpp"
#include "pkg/dependency_solver.hpp"

namespace tinexus::pkg {

class PackageManager {
public:
    PackageManager() = default;
    ~PackageManager() = default;

    bool install_package(const PackageManifest& manifest);
    bool remove_package(const std::string& name);
    [[nodiscard]] bool is_installed(const std::string& name) const;
    [[nodiscard]] const PackageDatabase& database() const noexcept { return m_db; }

private:
    PackageDatabase m_db;
    TransactionManager m_tx;
    DependencySolver m_solver;
};

} // namespace tinexus::pkg

#endif // TINEXUS_PKG_PACKAGE_MANAGER_HPP
