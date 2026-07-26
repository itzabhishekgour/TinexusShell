#ifndef TINEXUS_LAUNCHER_IPC_CLIENT_HPP
#define TINEXUS_LAUNCHER_IPC_CLIENT_HPP

#include <string>
#include <functional>
#include "launcher/search_model.hpp"

namespace tinexus::launcher {

class IPCClient {
public:
    static IPCClient& instance() noexcept;

    IPCClient() = default;
    ~IPCClient() = default;

    using ResultsCallback = std::function<void(const std::vector<LauncherResultItem>& items)>;
    using ToggleCallback = std::function<void()>;

    void set_results_callback(ResultsCallback cb);
    void set_toggle_callback(ToggleCallback cb);

    void send_search_query(const std::string& query);
    void send_activate_item(const std::string& result_id);

    // Mock/Simulate IPC reception for testing
    void receive_mock_results(const std::vector<LauncherResultItem>& items);
    void receive_shortcut_toggle();

private:
    ResultsCallback m_results_cb{nullptr};
    ToggleCallback m_toggle_cb{nullptr};
};

} // namespace tinexus::launcher

#endif // TINEXUS_LAUNCHER_IPC_CLIENT_HPP
