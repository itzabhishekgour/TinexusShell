#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "monitor/resource_monitor.hpp"
#include "monitor/cpu_parser.hpp"
#include "monitor/memory_parser.hpp"
#include "monitor/disk_parser.hpp"
#include "monitor/network_parser.hpp"
#include "monitor/process_tree.hpp"
#include "monitor/process_controller.hpp"

void test_proc_memory_parsing() {
    auto mem = tinexus::monitor::MemoryParser::parse_memory();
    assert(mem.total_ram_bytes > 0);
    assert(mem.available_ram_bytes > 0);
    std::cout << "[PASS] test_proc_memory_parsing\n";
}

void test_proc_cpu_parsing() {
    tinexus::monitor::CpuParser parser;
    auto cores = parser.parse_cpu_usage();
    assert(!cores.empty());
    std::cout << "[PASS] test_proc_cpu_parsing\n";
}

void test_process_tree_discovery() {
    auto procs = tinexus::monitor::ProcessTree::discover_processes();
    assert(!procs.empty());

    bool found_init_or_systemd = false;
    for (const auto& proc : procs) {
        if (proc.pid == 1) {
            found_init_or_systemd = true;
            break;
        }
    }
    assert(found_init_or_systemd);
    std::cout << "[PASS] test_process_tree_discovery\n";
}

void test_resource_monitor_snapshot() {
    auto& monitor = tinexus::monitor::ResourceMonitor::instance();
    auto snap = monitor.collect_snapshot();
    assert(!snap.cpu_cores.empty());
    assert(snap.memory.total_ram_bytes > 0);

    auto hist = monitor.history();
    assert(!hist.empty());
    std::cout << "[PASS] test_resource_monitor_snapshot\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_monitor");
    tinexus::log::info("Running Integration Test Suite for System Monitor...");

    test_proc_memory_parsing();
    test_proc_cpu_parsing();
    test_process_tree_discovery();
    test_resource_monitor_snapshot();

    tinexus::log::info("All System Monitor integration tests passed 100%!");
    return 0;
}
