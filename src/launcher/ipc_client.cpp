#include "launcher/ipc_client.hpp"
#include "common/logger.hpp"

namespace tinexus::launcher {

IPCClient& IPCClient::instance() noexcept {
    static IPCClient s_instance;
    return s_instance;
}

void IPCClient::set_results_callback(ResultsCallback cb) {
    m_results_cb = std::move(cb);
}

void IPCClient::set_toggle_callback(ToggleCallback cb) {
    m_toggle_cb = std::move(cb);
}

void IPCClient::send_search_query(const std::string& query) {
    log::info("IPCClient: Sending SEARCH_QUERY -> '{}'", query);

    // In full deployment, writes protocol::Header + payload to /run/user/$UID/tinexus/ipc.sock
    // For unit testing & demo, trigger instant response
    std::vector<LauncherResultItem> mock_results;

    if (query.find("2+") == 0 || query.find("10*") == 0 || query.find("5*") == 0) {
        mock_results.push_back({"calc_1", "10", "= 10 (Calculator)", "accessories-calculator", "Calculator", "10"});
    } else if (query == "lock" || query == "shutdown" || query == "reboot") {
        mock_results.push_back({"sys_1", query, "System Action", "system-shutdown", "System", query});
    } else if (!query.empty()) {
        mock_results.push_back({"app_firefox", "Firefox Web Browser", "Web Browser", "org.mozilla.firefox", "Applications", "firefox"});
        mock_results.push_back({"app_terminal", "Terminal", "System Terminal", "org.gnome.Terminal", "Applications", "gnome-terminal"});
    }

    receive_mock_results(mock_results);
}

void IPCClient::send_activate_item(const std::string& result_id) {
    log::info("IPCClient: Sending ACTIVATE_ITEM -> '{}'", result_id);
}

void IPCClient::receive_mock_results(const std::vector<LauncherResultItem>& items) {
    if (m_results_cb) {
        m_results_cb(items);
    }
}

void IPCClient::receive_shortcut_toggle() {
    log::info("IPCClient: Received SHORTCUT_ACTIVATED signal (Ctrl+K)");
    if (m_toggle_cb) {
        m_toggle_cb();
    }
}

} // namespace tinexus::launcher
