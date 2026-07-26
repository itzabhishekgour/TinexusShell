#ifndef TINEXUS_SERVICED_DEP_GRAPH_HPP
#define TINEXUS_SERVICED_DEP_GRAPH_HPP

#include "serviced/daemon_spec.hpp"
#include <vector>
#include <unordered_map>
#include <string>

namespace tinexus::serviced {

class DependencyGraph {
public:
    DependencyGraph() = default;

    void add_service(DaemonSpec spec);
    [[nodiscard]] bool has_cycle() const;
    [[nodiscard]] std::vector<std::string> get_startup_order() const;
    [[nodiscard]] std::vector<std::string> get_shutdown_order() const;
    [[nodiscard]] std::optional<DaemonSpec> get_spec(const std::string& service_id) const;

private:
    std::unordered_map<std::string, DaemonSpec> m_services;

    bool dfs_cycle_check(const std::string& node, 
                         std::unordered_map<std::string, int>& visited) const;
};

} // namespace tinexus::serviced

#endif // TINEXUS_SERVICED_DEP_GRAPH_HPP
