#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "common/action_request.hpp"
#include "serviced/launch_authority.hpp"
#include "serviced/dep_graph.hpp"
#include "serviced/event_journal.hpp"
#include "serviced/heartbeat_watchdog.hpp"
#include <unistd.h>
#include <sys/wait.h>

void test_launch_authority_validation() {
    auto& authority = tinexus::serviced::LaunchAuthority::instance();

    // Valid commands
    assert(authority.is_valid_executable("firefox"));
    assert(authority.is_valid_executable("/usr/bin/gnome-terminal --dir=/home"));

    // Malicious injection strings MUST be rejected
    assert(!authority.is_valid_executable("firefox; rm -rf /"));
    assert(!authority.is_valid_executable("terminal | cat /etc/passwd"));
    assert(!authority.is_valid_executable("app & background_job"));

    std::cout << "[PASS] test_launch_authority_validation\n";
}

void test_structured_action_request() {
    auto& authority = tinexus::serviced::LaunchAuthority::instance();

    tinexus::ActionRequest req;
    req.type = tinexus::ActionType::RunCommand;
    req.target = "true";

    pid_t pid = authority.execute_action(req);
    assert(pid > 0);

    int status = 0;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);

    std::cout << "[PASS] test_structured_action_request\n";
}

void test_service_ping_pong() {
    tinexus::serviced::EventJournal::instance().log_event("searchd", "PING", "342 us");
    auto events = tinexus::serviced::EventJournal::instance().get_recent_events(5);
    assert(!events.empty());
    assert(events.back().event_type == "PING");

    std::cout << "[PASS] test_service_ping_pong\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_runtime");
    tinexus::log::info("Running integration test suite for Platform Runtime...");

    test_launch_authority_validation();
    test_structured_action_request();
    test_service_ping_pong();

    tinexus::log::info("All runtime integration tests passed successfully!");
    return 0;
}
