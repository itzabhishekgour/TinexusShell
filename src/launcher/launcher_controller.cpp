#include "launcher/launcher_controller.hpp"
#include "launcher/ipc_client.hpp"
#include "common/logger.hpp"

namespace tinexus::launcher {

LauncherController& LauncherController::instance() noexcept {
    static LauncherController s_instance;
    return s_instance;
}

LauncherController::LauncherController() {
    IPCClient::instance().set_results_callback([this](const std::vector<LauncherResultItem>& items) {
        m_model.set_items(items);
    });

    IPCClient::instance().set_toggle_callback([this]() {
        toggle_visibility();
    });
}

bool LauncherController::is_visible() const noexcept {
    return m_visible;
}

void LauncherController::show() {
    if (!m_visible) {
        m_visible = true;
        log::info("LauncherController: UI set to VISIBLE");
    }
}

void LauncherController::hide() {
    if (m_visible) {
        m_visible = false;
        m_model.clear();
        log::info("LauncherController: UI set to HIDDEN");
    }
}

void LauncherController::toggle_visibility() {
    if (m_visible) {
        hide();
    } else {
        show();
    }
}

void LauncherController::on_search_text_changed(const std::string& query) {
    if (query.empty()) {
        m_model.clear();
        return;
    }
    IPCClient::instance().send_search_query(query);
}

void LauncherController::activate_selected_item(size_t index) {
    const auto* item = m_model.get_item(index);
    if (item) {
        log::info("LauncherController: Activating selected item '{}'", item->title);
        IPCClient::instance().send_activate_item(item->id);
        hide();
    }
}

const SearchModel& LauncherController::model() const noexcept {
    return m_model;
}

} // namespace tinexus::launcher
