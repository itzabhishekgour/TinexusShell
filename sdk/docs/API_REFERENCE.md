# Tinexus Developer SDK — API Reference (`libtinexus-sdk.so`)

## Overview
The Tinexus Developer SDK provides C++20 object-oriented facades for third-party and first-party applications to interact with Tinexus Platform services (`searchd`, `serviced`, `comp`, `launcher`) over versioned IPC wire transport.

## Public Classes & Methods

### `tinexus::Client`
Unified client facade managing connection lifecycle and platform services.
- `Result<bool> connect(const std::string& endpoint = "/run/user/1000/tinexus/ipc.sock")`
- `void disconnect()`
- `bool is_connected() const noexcept`
- `SearchService& search() noexcept`
- `ActionService& actions() noexcept`

### `tinexus::SearchService`
- `Result<std::vector<SearchResultItem>> query(const std::string& term)`
- `std::future<Result<std::vector<SearchResultItem>>> query_async(const std::string& term)`

### `tinexus::ActionService`
- `Result<bool> execute(const ActionRequest& req)`
- `std::future<Result<bool>> execute_async(const ActionRequest& req)`
