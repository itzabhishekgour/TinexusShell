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

    // Valid commands: validate_and_get_fd returns -2 (base system binary) or >=0 (secured app)
    // Both are non-(-1) results, meaning not rejected.
    assert(authority.validate_and_get_fd("firefox") != -1);
    assert(authority.validate_and_get_fd("/usr/bin/gnome-terminal") != -1);

    // Malicious injection strings MUST be rejected (return -1)
    assert(authority.validate_and_get_fd("firefox; rm -rf /") == -1);
    assert(authority.validate_and_get_fd("terminal | cat /etc/passwd") == -1);
    assert(authority.validate_and_get_fd("app & background_job") == -1);

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
