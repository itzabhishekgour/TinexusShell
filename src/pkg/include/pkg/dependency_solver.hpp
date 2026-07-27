#ifndef TINEXUS_PKG_DEPENDENCY_SOLVER_HPP
#define TINEXUS_PKG_DEPENDENCY_SOLVER_HPP

#include "pkg/package_manifest.hpp"
#include <vector>
#include <unordered_map>

namespace tinexus::pkg {

class DependencySolver {
public:
    DependencySolver() = default;
    ~DependencySolver() = default;

    bool resolve(const std::vector<PackageManifest>& available_packages,
                 const std::string& target_package,
                 std::vector<std::string>& out_install_order);
};

} // namespace tinexus::pkg

#endif // TINEXUS_PKG_DEPENDENCY_SOLVER_HPP
