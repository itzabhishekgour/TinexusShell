#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "notifications/notification_server.hpp"
#include "notifications/notification_manager.hpp"
#include "notifications/dnd_manager.hpp"

void test_get_capabilities_and_info() {
    auto& server = tinexus::notifications::NotificationServer::instance();
    auto caps = server.get_capabilities();
    assert(!caps.empty());
    assert(caps[0] == "actions");

    auto info = server.get_server_information();
    assert(info.name == "tinexus-notifications");
    std::cout << "[PASS] test_get_capabilities_and_info\n";
}

void test_notify_creation_and_retrieval() {
    auto& server = tinexus::notifications::NotificationServer::instance();
    auto id = server.notify("TestApp", 0, "icon", "Test Summary", "Test Body", {}, tinexus::notifications::Urgency::Normal, 5000);
    assert(id > 0);

    auto notif = tinexus::notifications::NotificationManager::instance().get_notification(id);
    assert(notif.has_value());
    assert(notif->summary == "Test Summary");
    std::cout << "[PASS] test_notify_creation_and_retrieval\n";
}

void test_notification_replacement_by_id() {
    auto& server = tinexus::notifications::NotificationServer::instance();
    auto original_id = server.notify("Downloader", 0, "icon", "Downloading 20%", "Progress...", {}, tinexus::notifications::Urgency::Normal, 5000);
    assert(original_id > 0);

    auto updated_id = server.notify("Downloader", original_id, "icon", "Downloading 60%", "Progress...", {}, tinexus::notifications::Urgency::Normal, 5000);
    assert(updated_id == original_id);

    auto notif = tinexus::notifications::NotificationManager::instance().get_notification(original_id);
    assert(notif.has_value());
    assert(notif->summary == "Downloading 60%");
    std::cout << "[PASS] test_notification_replacement_by_id\n";
}

void test_dnd_policy_suppression() {
    auto& dnd = tinexus::notifications::DndManager::instance();
    dnd.set_dnd_reason(tinexus::notifications::DndReason::UserEnabled);

    // Normal urgency suppressed
    assert(dnd.should_suppress_popup(tinexus::notifications::Urgency::Normal));
    // Critical urgency bypasses DND
    assert(!dnd.should_suppress_popup(tinexus::notifications::Urgency::Critical));

    dnd.set_dnd_reason(tinexus::notifications::DndReason::None);
    assert(!dnd.should_suppress_popup(tinexus::notifications::Urgency::Normal));

    std::cout << "[PASS] test_dnd_policy_suppression\n";
}

void test_close_notification_and_ring_history() {
    auto& server = tinexus::notifications::NotificationServer::instance();
    auto id = server.notify("System", 0, "icon", "Alert", "Critical Body", {}, tinexus::notifications::Urgency::Critical, 0);
    assert(server.close_notification(id));

    assert(!tinexus::notifications::NotificationManager::instance().get_notification(id).has_value());

    auto hist = tinexus::notifications::NotificationManager::instance().history();
    assert(!hist.empty());

    std::cout << "[PASS] test_close_notification_and_ring_history\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_notifications");
    tinexus::log::info("Running Integration Test Suite for Notification Center...");

    test_get_capabilities_and_info();
    test_notify_creation_and_retrieval();
    test_notification_replacement_by_id();
    test_dnd_policy_suppression();
    test_close_notification_and_ring_history();

    tinexus::log::info("All Notification Center integration tests passed 100%!");
    return 0;
}
