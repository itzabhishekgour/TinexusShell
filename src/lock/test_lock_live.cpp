// ============================================================================
// test_lock_live.cpp — Live PAM Authentication & State Tests for tinexus-lock
// ============================================================================
#include "LockBridge.hpp"
#include <QtGui/QGuiApplication>
#include <iostream>
#include <cassert>

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("test-lock-live"));

    std::cout << "=== Tinexus Lock Screen PAM & State Verification Suite ===" << std::endl;

    tinexus::lock::LockBridge bridge;

    // 1. User and Clock properties
    std::cout << "\n[TEST 1] Verifying User & Clock Metadata..." << std::endl;
    std::cout << "  Current user: '" << bridge.username().toStdString() << "'" << std::endl;
    std::cout << "  Current time: '" << bridge.currentTime().toStdString() << "'" << std::endl;
    std::cout << "  Current date: '" << bridge.currentDate().toStdString() << "'" << std::endl;
    assert(!bridge.username().isEmpty());
    assert(!bridge.currentTime().isEmpty());
    assert(!bridge.currentDate().isEmpty());
    std::cout << "  -> PASS: User and Clock metadata verified." << std::endl;

    // 2. Real PAM Authentication with Wrong Password
    std::cout << "\n[TEST 2] Verifying Real PAM Rejection of Wrong Password..." << std::endl;
    assert(!bridge.authFailed());
    assert(!bridge.isAuthenticating());

    bool authStateFired = false;
    bool authFailedFired = false;
    bool unlockSuccessFired = false;

    QObject::connect(&bridge, &tinexus::lock::LockBridge::authStateChanged, [&]() {
        authStateFired = true;
    });
    QObject::connect(&bridge, &tinexus::lock::LockBridge::authFailedChanged, [&]() {
        authFailedFired = true;
    });
    QObject::connect(&bridge, &tinexus::lock::LockBridge::unlockSuccess, [&]() {
        unlockSuccessFired = true;
    });

    std::cout << "  Calling bridge.authenticate(\"invalid_test_password_9999\")..." << std::endl;
    bridge.authenticate(QStringLiteral("invalid_test_password_9999"));

    std::cout << "  Result: authFailed = " << (bridge.authFailed() ? "TRUE" : "FALSE")
              << ", isAuthenticating = " << (bridge.isAuthenticating() ? "TRUE" : "FALSE")
              << ", unlockSuccessFired = " << (unlockSuccessFired ? "TRUE" : "FALSE") << std::endl;

    assert(bridge.authFailed());
    assert(!bridge.isAuthenticating());
    assert(authStateFired);
    assert(authFailedFired);
    assert(!unlockSuccessFired);
    std::cout << "  -> PASS: Real PAM rejected incorrect credentials without bypassing!" << std::endl;

    // 3. Reset failed state
    std::cout << "\n[TEST 3] Verifying Auth Reset Functionality..." << std::endl;
    bridge.resetAuthFailed();
    assert(!bridge.authFailed());
    std::cout << "  -> PASS: ResetAuthFailed verified." << std::endl;

    // 4. Empty password / Enter-to-unlock verification (live ISO behavior)
    std::cout << "\n[TEST 4] Verifying Empty Password / Enter-to-unlock..." << std::endl;
    bool emptyUnlockFired = false;
    QObject::connect(&bridge, &tinexus::lock::LockBridge::unlockSuccess, [&]() {
        emptyUnlockFired = true;
    });
    bridge.authenticate(QStringLiteral(""));
    assert(!bridge.authFailed());
    assert(emptyUnlockFired);
    std::cout << "  -> PASS: Pressing Enter with empty password successfully unlocks session!" << std::endl;

    std::cout << "\n>>> ALL LOCK UNIT & PAM CONTRACT VERIFICATIONS PASSED! <<<" << std::endl;
    return 0;
}
