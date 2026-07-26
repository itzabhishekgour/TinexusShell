#include "tinexus/client.hpp"
#include "common/logger.hpp"
#include <iostream>

namespace tinexus {

class Client::Impl {
public:
    bool connected{false};
    SearchService search_service;
    ActionService action_service;
};

Client::Client() : m_impl(std::make_unique<Impl>()) {}
Client::~Client() = default;

Result<bool> Client::connect(const std::string& endpoint) {
    log::info("SDK Client: Negotiating connection with platform IPC at '{}'...", endpoint);
    m_impl->connected = true;
    return Result<bool>(true);
}

void Client::disconnect() {
    log::info("SDK Client: Disconnected from platform.");
    m_impl->connected = false;
}

bool Client::is_connected() const noexcept {
    return m_impl->connected;
}

SearchService& Client::search() noexcept {
    return m_impl->search_service;
}

ActionService& Client::actions() noexcept {
    return m_impl->action_service;
}

Result<std::vector<SearchResultItem>> SearchService::query(const std::string& term) {
    log::info("SDK SearchService: Querying term '{}'", term);
    std::vector<SearchResultItem> items;
    items.emplace_back("app-firefox", "Firefox Web Browser", "Applications", 0.98);
    return Result<std::vector<SearchResultItem>>(items);
}

std::future<Result<std::vector<SearchResultItem>>> SearchService::query_async(const std::string& term) {
    return std::async(std::launch::async, [this, term]() {
        return query(term);
    });
}

Result<bool> ActionService::execute(const ActionRequest& req) {
    log::info("SDK ActionService: Dispatching action target '{}'", req.target);
    return Result<bool>(true);
}

std::future<Result<bool>> ActionService::execute_async(const ActionRequest& req) {
    return std::async(std::launch::async, [this, req]() {
        return execute(req);
    });
}

} // namespace tinexus
