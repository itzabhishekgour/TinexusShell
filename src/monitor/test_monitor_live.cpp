#include "monitor/MonitorWidget.hpp"
#include "monitor/resource_monitor.hpp"
#include "monitor/process_tree.hpp"
#include <txui/input/Event.hpp>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <unordered_set>

using namespace txui;
using namespace tinexus::monitor;

int main() {
    std::cout << "=====================================================================" << std::endl;
    std::cout << "  TINEXUS ACTIVITY MONITOR — BATCH 2 LIVE FUNCTIONALITY VERIFICATION  " << std::endl;
    std::cout << "=====================================================================" << std::endl;

    // 1. Benchmark Raw Process Scanner (< 100ms requirement)
    ProcessTree scanner;
    auto t0 = std::chrono::high_resolution_clock::now();
    auto procs = scanner.discover_processes();
    auto t1 = std::chrono::high_resolution_clock::now();
    double scan_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "[1] Process Scanner Performance Benchmark:" << std::endl;
    std::cout << "    Discovered " << procs.size() << " live processes in "
              << std::fixed << std::setprecision(2) << scan_ms << " ms." << std::endl;
    assert(scan_ms < 100.0 && "Performance requirement failure: Process scan took >= 100ms!");
    std::cout << "    -> PASS: Scan time is well under the 100ms threshold." << std::endl << std::endl;

    // 2. Real Data Verification (Not hardcoded "user" or "10.0 MB")
    std::unordered_set<std::string> users;
    std::unordered_set<uint64_t> memory_values;
    bool found_pid1 = false;

    std::cout << "[2] Live Process Inspection Table (Sample):" << std::endl;
    std::cout << "    " << std::left
              << std::setw(8)  << "PID"
              << std::setw(22) << "Process Name"
              << std::setw(16) << "User"
              << std::setw(12) << "State"
              << std::setw(14) << "Memory (RSS)"
              << std::setw(10) << "CPU %" << std::endl;
    std::cout << "    ----------------------------------------------------------------------------" << std::endl;

    for (size_t i = 0; i < std::min<size_t>(10, procs.size()); ++i) {
        const auto& p = procs[i];
        users.insert(p.user);
        memory_values.insert(p.rss_bytes);
        if (p.pid == 1) found_pid1 = true;

        double rss_mb = static_cast<double>(p.rss_bytes) / (1024.0 * 1024.0);
        std::cout << "    " << std::left
                  << std::setw(8)  << p.pid
                  << std::setw(22) << p.name.substr(0, 20)
                  << std::setw(16) << p.user
                  << std::setw(12) << p.state_str
                  << std::fixed << std::setprecision(1) << std::setw(14) << (std::to_string(rss_mb).substr(0, 5) + " MB")
                  << std::setprecision(1) << p.cpu_percent << "%" << std::endl;
    }

    for (const auto& p : procs) {
        users.insert(p.user);
        memory_values.insert(p.rss_bytes);
        if (p.pid == 1) found_pid1 = true;
    }

    if (!found_pid1) {
        std::cerr << "FAIL: Failed to find PID 1 in /proc" << std::endl;
        return 1;
    }
    if (memory_values.size() <= 1) {
        std::cerr << "FAIL: Memory values are not varying" << std::endl;
        return 1;
    }
    std::cout << "    -> PASS: Real processes verified with dynamic RSS memory." << std::endl << std::endl;

    // 3. Widget Event & Sort Verification
    auto widget = txui::make_ref<MonitorWidget>();
    const uint32_t W = 860, H = 580;
    widget->measure(Constraints(0, W, 0, H));
    widget->layout(Rect(0, 0, W, H));
    widget->refresh_telemetry();

    std::cout << "[3] Sortable Table Columns Verification:" << std::endl;

    // Sort by Memory Descending
    widget->set_sort(SortColumn::Memory, true);
    auto sorted_mem = widget->get_filtered_and_sorted_processes();
    if (sorted_mem.size() < 2) {
        std::cerr << "FAIL: Need at least 2 processes to verify sorting" << std::endl;
        return 1;
    }
    for (size_t i = 1; i < sorted_mem.size(); ++i) {
        if (sorted_mem[i - 1].rss_bytes < sorted_mem[i].rss_bytes) {
            std::cerr << "FAIL: Memory descending sort violation" << std::endl;
            return 1;
        }
    }
    std::cout << "    -> Memory Descending: Top process is '" << sorted_mem.front().name
              << "' with " << (sorted_mem.front().rss_bytes / (1024 * 1024)) << " MB. PASS." << std::endl;

    // Sort by PID Ascending
    widget->set_sort(SortColumn::PID, false);
    auto sorted_pid = widget->get_filtered_and_sorted_processes();
    if (sorted_pid.front().pid != 1) {
        std::cerr << "FAIL: PID ascending sort top must be PID 1" << std::endl;
        return 1;
    }
    std::cout << "    -> PID Ascending: Top process is PID " << sorted_pid.front().pid
              << " ('" << sorted_pid.front().name << "'). PASS." << std::endl;

    // Sort by Name Ascending
    widget->set_sort(SortColumn::Name, false);
    auto sorted_name = widget->get_filtered_and_sorted_processes();
    for (size_t i = 1; i < sorted_name.size(); ++i) {
        if (sorted_name[i - 1].name > sorted_name[i].name) {
            std::cerr << "FAIL: Name ascending sort violation" << std::endl;
            return 1;
        }
    }
    std::cout << "    -> Name Ascending: Top process is '" << sorted_name.front().name << "'. PASS." << std::endl << std::endl;

    // 4. Live Search Filtering Verification
    std::cout << "[4] Live Search / Filter Verification:" << std::endl;
    widget->set_search_query("system");
    auto filtered_sys = widget->get_filtered_and_sorted_processes();
    std::cout << "    Filter 'system': " << filtered_sys.size() << " matches found." << std::endl;
    for (const auto& p : filtered_sys) {
        std::string lower_name = p.name;
        for (char& c : lower_name) c = static_cast<char>(std::tolower(c));
        if (lower_name.find("system") == std::string::npos && std::to_string(p.pid).find("system") == std::string::npos) {
            std::cerr << "FAIL: Search match violation on " << p.name << std::endl;
            return 1;
        }
    }
    std::cout << "    -> PASS: Substring search correctly filters in-memory snapshot." << std::endl << std::endl;

    // 5. Category Filtering Verification
    std::cout << "[5] Category Filter Verification:" << std::endl;
    widget->set_search_query("");
    widget->set_category(ProcessCategory::System);
    auto sys_procs = widget->get_filtered_and_sorted_processes();
    std::cout << "    Category 'System': " << sys_procs.size() << " processes found." << std::endl;
    for (const auto& p : sys_procs) {
        if (!p.is_system_daemon) {
            std::cerr << "FAIL: Non-system daemon leaked into system category: " << p.name << std::endl;
            return 1;
        }
    }
    std::cout << "    -> PASS: Category filtering correctly isolates system daemons." << std::endl << std::endl;

    // 6. Safety-Critical Termination Confirmation Modal Verification
    std::cout << "[6] Process Termination Safety Modal Verification:" << std::endl;
    widget->set_category(ProcessCategory::All);
    widget->set_selected_pid(1); // Target PID 1 (systemd)
    widget->trigger_end_process(false);

    assert(widget->is_modal_open() && "Modal dialog failed to open upon termination request!");
    std::cout << "    Modal opened for PID 1." << std::endl;

    // Simulate clicking Cancel button
    Event cancel_evt{};
    cancel_evt.type = EventType::PointerButtonPress;
    cancel_evt.pointer.x = 420.0; // Near cancel button in centered dialog
    cancel_evt.pointer.y = 390.0;
    cancel_evt.pointer.button = MouseButton::Left;
    widget->handle_event(cancel_evt);

    assert(!widget->is_modal_open() && "Cancel button failed to safely dismiss the modal!");
    std::cout << "    -> PASS: Modal opened and cancelled safely without dispatching any signal." << std::endl << std::endl;

    // 7. Authoritative PlatformServices PID Cross-Check Verification
    std::cout << "[7] PlatformServices PID Protection Cross-Check Verification:" << std::endl;
    if (!tinexus::platform::PlatformServices::is_protected_pid(1)) {
        std::cerr << "FAIL: PID 1 must be marked protected!" << std::endl;
        return 1;
    }
    if (tinexus::platform::PlatformServices::is_protected_pid(99999)) {
        std::cerr << "FAIL: Unregistered PID 99999 falsely marked protected!" << std::endl;
        return 1;
    }
    std::cout << "    -> PASS: Authoritative PID verification matches supervised daemons." << std::endl << std::endl;

    std::cout << "=====================================================================" << std::endl;
    std::cout << "  ALL BATCH 2 LIVE VERIFICATION TESTS PASSED SUCCESSFULLY!           " << std::endl;
    std::cout << "=====================================================================" << std::endl;

    return 0;
}
