#include <iostream>
#include <cassert>
#include "tinexus/client.hpp"

void test_sdk_client_connection() {
    tinexus::Client client;
    assert(!client.is_connected());

    auto conn_res = client.connect();
    assert(conn_res.is_ok());
    assert(conn_res.value() == true);
    assert(client.is_connected());

    client.disconnect();
    assert(!client.is_connected());

    std::cout << "[PASS] test_sdk_client_connection\n";
}

void test_sdk_search_service() {
    tinexus::Client client;
    client.connect();

    auto res = client.search().query("firefox");
    assert(res.is_ok());
    assert(!res.value().empty());
    assert(res.value().front().title() == "Firefox Web Browser");

    // Async query test
    auto fut = client.search().query_async("firefox");
    auto async_res = fut.get();
    assert(async_res.is_ok());
    assert(!async_res.value().empty());

    std::cout << "[PASS] test_sdk_search_service (sync & async)\n";
}

void test_sdk_action_service() {
    tinexus::Client client;
    client.connect();

    tinexus::ActionRequest req;
    req.type = tinexus::ActionType::AppLaunch;
    req.target = "firefox";

    auto res = client.actions().execute(req);
    assert(res.is_ok());
    assert(res.value() == true);

    std::cout << "[PASS] test_sdk_action_service\n";
}

int main() {
    std::cout << "Running Developer SDK Unit Test Suite...\n";
    test_sdk_client_connection();
    test_sdk_search_service();
    test_sdk_action_service();
    std::cout << "All SDK unit tests passed 100%!\n";
    return 0;
}
