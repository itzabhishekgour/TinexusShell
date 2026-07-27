#ifndef TINEXUS_MONITOR_PROCESS_TREE_HPP
#define TINEXUS_MONITOR_PROCESS_TREE_HPP

#include "monitor/metrics_snapshot.hpp"
#include <vector>

namespace tinexus::monitor {

class ProcessTree {
public:
    static std::vector<ProcessInfo> discover_processes();
};

} // namespace tinexus::monitor

#endif // TINEXUS_MONITOR_PROCESS_TREE_HPP
