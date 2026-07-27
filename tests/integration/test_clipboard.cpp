#include <iostream>
#include <cassert>
#include "common/logger.hpp"
#include "clipboard/privacy_filter.hpp"
#include "clipboard/clipboard_manager.hpp"

void test_privacy_filter() {
    assert(tinexus::clipboard::PrivacyFilter::is_sensitive("Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"));
    assert(tinexus::clipboard::PrivacyFilter::is_sensitive("password=secret123"));
    assert(tinexus::clipboard::PrivacyFilter::is_sensitive("-----BEGIN PRIVATE KEY-----\nkey_data\n-----END PRIVATE KEY-----"));
    assert(!tinexus::clipboard::PrivacyFilter::is_sensitive("Hello Tinexus OS"));

    std::cout << "[PASS] test_privacy_filter\n";
}

void test_clipboard_add_and_duplicate() {
    auto& mgr = tinexus::clipboard::ClipboardManager::instance();

    // Sensitive items rejected
    assert(mgr.add_item("password=topsecret") == 0);

    auto id1 = mgr.add_item("https://tinexus.org", "text/plain");
    assert(id1 > 0);

    // Duplicate item increments copy count
    auto id2 = mgr.add_item("https://tinexus.org", "text/plain");
    assert(id2 == id1);

    auto hist = mgr.history();
    assert(!hist.empty());
    assert(hist[0].copy_count == 2);

    std::cout << "[PASS] test_clipboard_add_and_duplicate\n";
}

void test_clipboard_pinning() {
    auto& mgr = tinexus::clipboard::ClipboardManager::instance();
    auto id = mgr.add_item("Important Command: git status", "text/plain");
    assert(id > 0);

    assert(mgr.pin_item(id, true));
    auto pinned = mgr.pinned_items();
    assert(!pinned.empty());
    assert(pinned[0].id == id);

    assert(mgr.pin_item(id, false));
    assert(mgr.pinned_items().empty());

    std::cout << "[PASS] test_clipboard_pinning\n";
}

int main() {
    tinexus::log::set_component_name("integration_test_clipboard");
    tinexus::log::info("Running Integration Test Suite for Clipboard Manager...");

    test_privacy_filter();
    test_clipboard_add_and_duplicate();
    test_clipboard_pinning();

    tinexus::log::info("All Clipboard Manager integration tests passed 100%!");
    return 0;
}
