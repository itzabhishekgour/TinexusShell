// ============================================================================
// test_launcher_live.cpp — Live Functional Test for tinexus-launcher
// ============================================================================
#include "LauncherBridge.hpp"
#include <QtGui/QGuiApplication>
#include <iostream>
#include <cassert>

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("test-launcher-live"));

    std::cout << "=== Tinexus Launcher Live Functional Verification Suite ===" << std::endl;

    tinexus::launcher::LauncherBridge bridge;

    // 1. Initial State & Installed Apps Count
    std::cout << "\n[TEST 1] Verifying Dynamic App List & System Actions..." << std::endl;
    QVariantList allResults = bridge.resultsList();
    std::cout << "  Default items in launcher: " << allResults.size() << std::endl;
    assert(allResults.size() >= 10); // 5 core + 5 system actions + scanned desktop apps

    // Verify presence of System Actions (restored txui functionality)
    bool hasLock = false, hasRestart = false, hasShutDown = false;
    for (const auto& itemVar : allResults) {
        QVariantMap map = itemVar.toMap();
        if (map["name"].toString() == "Lock Screen") hasLock = true;
        if (map["name"].toString() == "Restart") hasRestart = true;
        if (map["name"].toString() == "Shut Down") hasShutDown = true;
    }
    std::cout << "  System actions found: Lock=" << (hasLock ? "YES" : "NO")
              << ", Restart=" << (hasRestart ? "YES" : "NO")
              << ", ShutDown=" << (hasShutDown ? "YES" : "NO") << std::endl;
    assert(hasLock && hasRestart && hasShutDown);
    std::cout << "  -> PASS: System actions verified in launcher." << std::endl;

    // 2. Search Query Filtering
    std::cout << "\n[TEST 2] Verifying Search Filtering..." << std::endl;
    bridge.setQuery(QStringLiteral("lock"));
    QVariantList lockResults = bridge.resultsList();
    std::cout << "  Query 'lock' returned " << lockResults.size() << " results." << std::endl;
    assert(!lockResults.isEmpty());
    assert(lockResults[0].toMap()["name"].toString() == "Lock Screen");
    assert(lockResults[0].toMap()["kind"].toString() == "System");
    std::cout << "  -> PASS: Query 'lock' ranked 'Lock Screen' first." << std::endl;

    // 3. Calculator Integration
    std::cout << "\n[TEST 3] Verifying Built-in Calculator Heuristic..." << std::endl;
    bridge.setQuery(QStringLiteral("128*4"));
    QVariantList calcResults = bridge.resultsList();
    assert(!calcResults.isEmpty());
    std::cout << "  Query '128*4' result: '" << calcResults[0].toMap()["name"].toString().toStdString() << "'" << std::endl;
    assert(calcResults[0].toMap()["name"].toString() == "512");
    assert(calcResults[0].toMap()["kind"].toString() == "Calculator");
    std::cout << "  -> PASS: Math expression dynamically evaluated." << std::endl;

    // 4. App Store Fallback Search
    std::cout << "\n[TEST 4] Verifying App Store Fallback on Zero Local Matches..." << std::endl;
    bridge.setQuery(QStringLiteral("nonexistentxyz1234"));
    QVariantList storeResults = bridge.resultsList();
    assert(storeResults.size() == 1);
    std::cout << "  Fallback item: '" << storeResults[0].toMap()["name"].toString().toStdString() << "'" << std::endl;
    assert(storeResults[0].toMap()["kind"].toString() == "Store");
    std::cout << "  -> PASS: App Store fallback triggered." << std::endl;

    std::cout << "\n>>> ALL LAUNCHER FUNCTIONAL VERIFICATIONS PASSED! <<<" << std::endl;
    return 0;
}
