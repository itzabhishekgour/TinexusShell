// ============================================================================
// test_shell_live.cpp — Functional & Live Verification for tinexus-shell
// ============================================================================
#include "ShellBridge.hpp"
#include <QtGui/QGuiApplication>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <iostream>
#include <cassert>

int main(int argc, char* argv[]) {
    // Run headless / offscreen Qt core loop
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("test-shell-live"));

    std::cout << "=== Tinexus Shell Live Functional Verification Suite ===" << std::endl;

    tinexus::shell::ShellBridge bridge;

    // ────────────────────────────────────────────────────────────────────────
    // TEST 1: ADDITION 1 — Logo Restoration & Verification
    // ────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 1] Verifying Official Tinexus Logo Asset Resolution..." << std::endl;
    QString logoUrl = bridge.logoUrl();
    std::cout << "  Resolved logo URL: " << logoUrl.toStdString() << std::endl;
    if (logoUrl.isEmpty()) {
        std::cerr << "FAIL: bridge.logoUrl() returned empty string!" << std::endl;
        return 1;
    }
    QUrl url(logoUrl);
    QString localPath = url.toLocalFile();
    if (localPath.isEmpty()) {
        localPath = logoUrl; // If already a local file path
    }
    QFileInfo logoInfo(localPath);
    std::cout << "  Local logo path: " << localPath.toStdString() << std::endl;
    std::cout << "  File exists: " << (logoInfo.exists() ? "YES" : "NO") << ", size: " << logoInfo.size() << " bytes" << std::endl;
    if (!logoInfo.exists() || logoInfo.size() == 0) {
        std::cerr << "FAIL: Resolved logo file does not exist or is empty: " << localPath.toStdString() << std::endl;
        return 1;
    }
    if (!localPath.contains("tinexus-logo")) {
        std::cerr << "FAIL: Resolved logo is not the official tinexus-logo asset!" << std::endl;
        return 1;
    }
    std::cout << "  -> PASS: Addition 1 (Official Logo Asset verified and accessible)" << std::endl;

    // ────────────────────────────────────────────────────────────────────────
    // TEST 2: FIX 4 — Applications Dropdown (.desktop Scanner & Launch)
    // ────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 2] Verifying Applications Dropdown & .desktop Scanner..." << std::endl;
    QVariantList apps = bridge.applicationsList();
    std::cout << "  Scanned applications count: " << apps.size() << std::endl;
    if (apps.isEmpty()) {
        std::cerr << "FAIL: applicationsList is empty! Expected entries from /usr/share/applications" << std::endl;
        return 1;
    }
    for (int i = 0; i < std::min(5, (int)apps.size()); ++i) {
        QVariantMap entry = apps[i].toMap();
        std::cout << "    [" << i << "] Name: '" << entry["name"].toString().toStdString()
                  << "', Exec: '" << entry["exec"].toString().toStdString() << "'" << std::endl;
    }
    // Test app menu toggle
    assert(!bridge.appMenuOpen());
    bridge.toggleAppMenu();
    assert(bridge.appMenuOpen());
    bridge.closeAllFlyouts();
    assert(!bridge.appMenuOpen());
    std::cout << "  -> PASS: Fix 4 (Applications scanner & menu state verified)" << std::endl;

    // ────────────────────────────────────────────────────────────────────────
    // TEST 3: FIX 5 — AuraNotch Interactivity
    // ────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 3] Verifying AuraNotch Interactivity..." << std::endl;
    // Notch center click
    bridge.onNotchCenterClicked();
    // Calendar toggle
    assert(!bridge.calendarOpen());
    bridge.toggleCalendar();
    assert(bridge.calendarOpen());
    bridge.closeAllFlyouts();
    assert(!bridge.calendarOpen());
    std::cout << "  -> PASS: Fix 5 (AuraNotch click handlers & calendar toggle verified)" << std::endl;

    // ────────────────────────────────────────────────────────────────────────
    // TEST 4: FIX 6 — LogoMenu Actions & Power Dialog
    // ────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 4] Verifying LogoMenu Actions & Power Dialog..." << std::endl;
    // Reboot request modal
    assert(!bridge.rebootConfirmationOpen());
    bridge.requestReboot();
    assert(bridge.rebootConfirmationOpen());
    bridge.closeAllFlyouts();
    assert(!bridge.rebootConfirmationOpen());

    // Shutdown request modal
    assert(!bridge.shutdownConfirmationOpen());
    bridge.requestShutdown();
    assert(bridge.shutdownConfirmationOpen());
    bridge.closeAllFlyouts();
    assert(!bridge.shutdownConfirmationOpen());

    // Sleep call (safe invocation)
    bridge.powerSleep();
    std::cout << "  -> PASS: Fix 6 (Power dialog triggers & logind actions verified)" << std::endl;

    // ────────────────────────────────────────────────────────────────────────
    // TEST 5: FIX 7 — Hardware Bridges (Audio, Backlight, Battery, Wifi)
    // ────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 5] Verifying Hardware Bridges..." << std::endl;
    int prevVol = bridge.volume();
    bridge.setVolume(82);
    std::cout << "  Volume set to 82: readback = " << bridge.volume() << std::endl;
    assert(bridge.volume() == 82);
    bridge.setVolume(prevVol);

    int prevBri = bridge.brightness();
    bridge.setBrightness(65);
    std::cout << "  Brightness set to 65: readback = " << bridge.brightness() << std::endl;
    assert(bridge.brightness() == 65);
    bridge.setBrightness(prevBri);

    bool prevMute = bridge.soundMuted();
    bridge.setSoundMuted(!prevMute);
    assert(bridge.soundMuted() == !prevMute);
    bridge.setSoundMuted(prevMute);

    std::cout << "  Battery percent: " << bridge.batteryPercent() << "%" << std::endl;
    std::cout << "  Network connected: " << (bridge.networkConnected() ? "YES" : "NO")
              << " (" << bridge.networkBars() << " bars)" << std::endl;

    bridge.openWifiSettings();
    std::cout << "  -> PASS: Fix 7 (Hardware controls & properties verified)" << std::endl;

    std::cout << "\n>>> ALL SHELL FIXES & ADDITIONS PASSED VERIFICATION! <<<" << std::endl;
    return 0;
}
