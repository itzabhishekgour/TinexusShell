#include "serviced/dep_graph.hpp"
#include "common/logger.hpp"
#include <algorithm>

namespace tinexus::serviced {

void DependencyGraph::add_service(DaemonSpec spec) {
    m_services[spec.id] = std::move(spec);
}

std::optional<DaemonSpec> DependencyGraph::get_spec(const std::string& service_id) const {
    auto it = m_services.find(service_id);
    if (it != m_services.end()) return it->second;
    return std::nullopt;
}

bool DependencyGraph::dfs_cycle_check(const std::string& node, std::unordered_map<std::string, int>& visited) const {
    visited[node] = 1; // Visiting (in stack)

    auto it = m_services.find(node);
    if (it != m_services.end()) {
        for (const auto& dep : it->second.hard_dependencies) {
            if (visited[dep] == 1) {
                return true; // Cycle found!
            }
            if (visited[dep] == 0) {
                if (dfs_cycle_check(dep, visited)) return true;
            }
        }
    }

    visited[node] = 2; // Visited
    return false;
}

bool DependencyGraph::has_cycle() const {
    std::unordered_map<std::string, int> visited;
    for (const auto& [id, spec] : m_services) {
        if (visited[id] == 0) {
            if (dfs_cycle_check(id, visited)) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> DependencyGraph::get_startup_order() const {
    std::vector<std::string> order;
    std::unordered_map<std::string, int> in_degree;

    for (const auto& [id, spec] : m_services) {
        in_degree[id] = static_cast<int>(spec.hard_dependencies.size());
    }

    std::vector<std::string> queue;
    for (const auto& [id, deg] : in_degree) {
        if (deg == 0) {
            queue.push_back(id);
        }
    }

    while (!queue.empty()) {
        std::string curr = queue.back();
        queue.pop_back();
        order.push_back(curr);

        for (const auto& [id, spec] : m_services) {
            for (const auto& dep : spec.hard_dependencies) {
                if (dep == curr) {
                    in_degree[id]--;
                    if (in_degree[id] == 0) {
                        queue.push_back(id);
                    }
                }
            }
        }
    }

    return order;
}

std::vector<std::string> DependencyGraph::get_shutdown_order() const {
    auto order = get_startup_order();
    std::reverse(order.begin(), order.end());
    return order;
}

} // namespace tinexus::serviced
