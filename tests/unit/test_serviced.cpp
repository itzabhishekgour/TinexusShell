#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "serviced/daemon_spec.hpp"
#include "serviced/dep_graph.hpp"
#include "serviced/process_manager.hpp"
#include "serviced/event_journal.hpp"
#include "serviced/heartbeat_watchdog.hpp"

void test_daemon_status_strings() {
    assert(tinexus::serviced::daemon_status_to_string(tinexus::serviced::DaemonStatus::Running) == "RUNNING");
    assert(tinexus::serviced::daemon_status_to_string(tinexus::serviced::DaemonStatus::Ready) == "READY");
    assert(tinexus::serviced::daemon_status_to_string(tinexus::serviced::DaemonStatus::Restarting) == "RESTARTING");
    std::cout << "[PASS] test_daemon_status_strings\n";
}

void test_dep_graph_topological_sort() {
    tinexus::serviced::DependencyGraph graph;

    tinexus::serviced::DaemonSpec ipcd;
    ipcd.id = "ipcd";
    graph.add_service(ipcd);

    tinexus::serviced::DaemonSpec searchd;
    searchd.id = "searchd";
    searchd.hard_dependencies = {"ipcd"};
    graph.add_service(searchd);

    tinexus::serviced::DaemonSpec launcher;
    launcher.id = "launcher";
    launcher.hard_dependencies = {"searchd"};
    graph.add_service(launcher);

    assert(!graph.has_cycle());

    auto startup = graph.get_startup_order();
    assert(startup.size() == 3);
    assert(startup[0] == "ipcd");
    assert(startup[1] == "searchd");
    assert(startup[2] == "launcher");

    auto shutdown = graph.get_shutdown_order();
    assert(shutdown.size() == 3);
    assert(shutdown[0] == "launcher");
    assert(shutdown[1] == "searchd");
    assert(shutdown[2] == "ipcd");

    std::cout << "[PASS] test_dep_graph_topological_sort\n";
}

void test_cycle_detection() {
    tinexus::serviced::DependencyGraph graph;

    tinexus::serviced::DaemonSpec service_a;
    service_a.id = "service_a";
    service_a.hard_dependencies = {"service_b"};
    graph.add_service(service_a);

    tinexus::serviced::DaemonSpec service_b;
    service_b.id = "service_b";
    service_b.hard_dependencies = {"service_a"};
    graph.add_service(service_b);

    assert(graph.has_cycle() == true);
    std::cout << "[PASS] test_cycle_detection\n";
}

void test_event_journal() {
    tinexus::serviced::EventJournal::instance().log_event("test_service", "STATE_CHANGE", "RUNNING");
    auto events = tinexus::serviced::EventJournal::instance().get_recent_events(10);
    assert(!events.empty());
    assert(events.back().service_id == "test_service");
    assert(events.back().event_type == "STATE_CHANGE");
    std::cout << "[PASS] test_event_journal\n";
}

int main() {
    tinexus::log::set_component_name("unit_test_serviced");
    tinexus::log::info("Running unit test suite for tinexus-serviced...");

    test_daemon_status_strings();
    test_dep_graph_topological_sort();
    test_cycle_detection();
    test_event_journal();

    tinexus::log::info("All serviced unit tests passed successfully!");
    return 0;
}
