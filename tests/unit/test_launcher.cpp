#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "launcher/theme_manager.hpp"
#include "launcher/search_model.hpp"
#include "launcher/ipc_client.hpp"
#include "launcher/launcher_controller.hpp"

void test_theme_manager() {
    auto& mgr = tinexus::launcher::ThemeManager::instance();
    mgr.set_dark_theme();
    auto dark_tokens = mgr.tokens();
    assert(dark_tokens.corner_radius == 12);
    assert(dark_tokens.background_surface == "#13131ACC");

    mgr.set_light_theme();
    auto light_tokens = mgr.tokens();
    assert(light_tokens.background_surface == "#FFFFFFCC");

    std::cout << "[PASS] test_theme_manager\n";
}

void test_search_model() {
    tinexus::launcher::SearchModel model;
    assert(model.count() == 0);

    std::vector<tinexus::launcher::LauncherResultItem> items = {
        {"1", "Firefox", "Web Browser", "firefox", "Applications", "firefox"},
        {"2", "Terminal", "System Shell", "terminal", "Applications", "gnome-terminal"}
    };

    model.set_items(items);
    assert(model.count() == 2);
    assert(model.get_item(0)->title == "Firefox");
    assert(model.get_item(1)->title == "Terminal");

    model.clear();
    assert(model.count() == 0);

    std::cout << "[PASS] test_search_model\n";
}

void test_launcher_controller() {
    auto& controller = tinexus::launcher::LauncherController::instance();
    controller.hide();
    assert(!controller.is_visible());

    controller.show();
    assert(controller.is_visible());

    controller.on_search_text_changed("firefox");
    assert(controller.model().count() > 0);

    controller.activate_selected_item(0);
    assert(!controller.is_visible()); // Hidden after activation

    std::cout << "[PASS] test_launcher_controller\n";
}

void test_ipc_client_shortcut() {
    auto& controller = tinexus::launcher::LauncherController::instance();
    controller.hide();
    assert(!controller.is_visible());

    // Trigger shortcut toggle signal (simulates Ctrl+K from compositor over ipcd)
    tinexus::launcher::IPCClient::instance().receive_shortcut_toggle();
    assert(controller.is_visible());

    tinexus::launcher::IPCClient::instance().receive_shortcut_toggle();
    assert(!controller.is_visible());

    std::cout << "[PASS] test_ipc_client_shortcut\n";
}

int main() {
    tinexus::log::set_component_name("unit_test_launcher");
    tinexus::log::info("Running unit test suite for tinexus-launcher...");

    test_theme_manager();
    test_search_model();
    test_launcher_controller();
    test_ipc_client_shortcut();

    tinexus::log::info("All tinexus-launcher unit tests passed successfully!");
    return 0;
}
