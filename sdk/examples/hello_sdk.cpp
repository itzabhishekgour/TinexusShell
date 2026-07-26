#include "tinexus/client.hpp"
#include <iostream>

int main() {
    std::cout << "--- Tinexus Developer SDK Demo ---\n";
    tinexus::Client client;

    auto conn_res = client.connect();
    if (!conn_res.is_ok()) {
        std::cerr << "Connection error: " << conn_res.error().message << "\n";
        return 1;
    }

    std::cout << "[SDK] Connected successfully!\n";

    // Perform Search Query
    auto search_res = client.search().query("firefox");
    if (search_res.is_ok()) {
        for (const auto& item : search_res.value()) {
            std::cout << " -> Result: " << item.title() << " [" << item.category() << "] (Score: " << item.score() << ")\n";
        }
    }

    // Dispatch ActionRequest
    tinexus::ActionRequest req;
    req.type = tinexus::ActionType::AppLaunch;
    req.target = "firefox";

    auto action_res = client.actions().execute(req);
    if (action_res.is_ok()) {
        std::cout << "[SDK] ActionRequest dispatched successfully!\n";
    }

    client.disconnect();
    std::cout << "--- SDK Demo Complete ---\n";
    return 0;
}
