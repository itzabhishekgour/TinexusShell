#include "pkg/dependency_solver.hpp"
#include "common/logger.hpp"
#include <unordered_set>

namespace tinexus::pkg {

bool DependencySolver::resolve(const std::vector<PackageManifest>& available_packages,
                                const std::string& target_package,
                                std::vector<std::string>& out_install_order) {
    std::unordered_map<std::string, PackageManifest> pkg_map;
    for (const auto& pkg : available_packages) {
        pkg_map[pkg.name] = pkg;
    }

    if (pkg_map.find(target_package) == pkg_map.end()) {
        log::error("DependencySolver: Target package '{}' not found in repository!", target_package);
        return false;
    }

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> visiting;

    auto dfs = [&](auto self, const std::string& name) -> bool {
        if (visiting.count(name)) {
            log::error("DependencySolver: Circular dependency detected involving '{}'!", name);
            return false;
        }
        if (visited.count(name)) return true;

        visiting.insert(name);
        if (pkg_map.count(name)) {
            for (const auto& dep : pkg_map[name].dependencies) {
                if (!self(self, dep)) return false;
            }
        }
        visiting.erase(name);
        visited.insert(name);
        out_install_order.push_back(name);
        return true;
    };

    bool ok = dfs(dfs, target_package);
    if (ok) {
        log::info("DependencySolver: Resolved installation DAG for '{}' (Steps: {})", target_package, out_install_order.size());
    }
    return ok;
}

} // namespace tinexus::pkg
