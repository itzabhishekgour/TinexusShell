#ifndef TINEXUS_LAUNCHER_IPC_CLIENT_HPP
#define TINEXUS_LAUNCHER_IPC_CLIENT_HPP

#include <string>
#include <functional>
#include <vector>
#include <atomic>
#include "launcher/search_model.hpp"

namespace tinexus::launcher {

// ----------------------------------------------------------------------------
// IPCClient — Launcher's connection to tinexus-ipcd.
//
// Lifecycle:
//   1. On first send_search_query(), lazily connects to ipcd Unix socket.
//   2. Registers itself as "launcher" with the broker.
//   3. Starts a background listener thread that reads SEARCH_RESULT frames
//      and dispatches them via the registered ResultsCallback.
//   4. Falls back to local_search_fallback() when ipcd is unavailable
//      (development / CI environments where ipcd is not running).
//
// Thread safety:
//   - send_search_query() and set_*_callback() should be called from the
//     UI (main) thread only.
//   - m_results_cb is invoked from the listener thread. The UI framework
//     (txui event loop) must handle cross-thread callback safely.
// ----------------------------------------------------------------------------
class IPCClient {
public:
    static IPCClient& instance() noexcept;

    IPCClient() = default;
    ~IPCClient() {
        m_listener_running = false;
        if (m_fd != -1) {
            ::close(m_fd);
            m_fd = -1;
        }
    }

    // Callbacks
    using ResultsCallback = std::function<void(const std::vector<LauncherResultItem>& items)>;
    using ToggleCallback  = std::function<void()>;

    void set_results_callback(ResultsCallback cb);
    void set_toggle_callback(ToggleCallback cb);

    // Public API
    void send_search_query(const std::string& query);
    void send_activate_item(const std::string& result_id);

    // Called by the listener thread; can also be used by tests
    void receive_mock_results(const std::vector<LauncherResultItem>& items);
    void receive_shortcut_toggle();

private:
    void start_listener_thread();
    void local_search_fallback(const std::string& query);

    ResultsCallback        m_results_cb{nullptr};
    ToggleCallback         m_toggle_cb{nullptr};
    int                    m_fd{-1};
    std::atomic<bool>      m_listener_running{false};
};

} // namespace tinexus::launcher

#endif // TINEXUS_LAUNCHER_IPC_CLIENT_HPP
