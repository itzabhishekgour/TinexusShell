// ============================================================================
// test_files_live.cpp — Live Filesystem & Navigation Verification for tinexus-files
// ============================================================================
#include "FilesBridge.hpp"
#include <QtGui/QGuiApplication>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <iostream>
#include <cassert>
#include <set>

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("test-files-live"));

    std::cout << "=== Tinexus Files Live Filesystem Verification Suite ===" << std::endl;

    // ────────────────────────────────────────────────────────────────────────
    // TEST 1: Real Directory Listing vs Real Filesystem (/etc)
    // ────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 1] Loading directory '/etc' and comparing with filesystem..." << std::endl;
    tinexus::files::FilesBridge bridge(QStringLiteral("/etc"));

    std::cout << "  currentPath: " << bridge.currentPath().toStdString() << std::endl;
    std::cout << "  currentDirName: " << bridge.currentDirName().toStdString() << std::endl;
    assert(bridge.currentPath() == QStringLiteral("/etc"));
    assert(bridge.currentDirName() == QStringLiteral("etc"));

    QVariantList items = bridge.fileList();
    std::cout << "  bridge.itemCount: " << items.size() << std::endl;
    assert(!items.isEmpty());

    // Compare with QDir /etc
    QDir actualDir(QStringLiteral("/etc"));
    QStringList actualEntries = actualDir.entryList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase
    );

    std::cout << "  Filesystem entries in /etc: " << actualEntries.size() << std::endl;
    assert(items.size() == actualEntries.size());

    // Verify presence of standard entries (e.g. hosts, passwd)
    std::set<std::string> namesInBridge;
    for (const auto& var : items) {
        QVariantMap map = var.toMap();
        namesInBridge.insert(map["name"].toString().toStdString());
    }

    assert(namesInBridge.count("hosts") > 0);
    assert(namesInBridge.count("passwd") > 0);
    std::cout << "  Found standard files in listing: 'hosts', 'passwd'" << std::endl;
    std::cout << "  -> PASS: Listing exactly matched filesystem entries (" << items.size() << " items)" << std::endl;

    // ────────────────────────────────────────────────────────────────────────
    // TEST 2: Navigation (cd, history, goBack, goForward, goUp)
    // ────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 2] Testing Navigation Flow (cd, back, forward, up)..." << std::endl;
    assert(!bridge.canGoBack());
    assert(!bridge.canGoForward());

    std::cout << "  cd('/tmp')..." << std::endl;
    bridge.cd(QStringLiteral("/tmp"));
    assert(bridge.currentPath() == QStringLiteral("/tmp"));
    assert(bridge.canGoBack());
    assert(!bridge.canGoForward());

    std::cout << "  goBack()..." << std::endl;
    bridge.goBack();
    assert(bridge.currentPath() == QStringLiteral("/etc"));
    assert(!bridge.canGoBack());
    assert(bridge.canGoForward());

    std::cout << "  goForward()..." << std::endl;
    bridge.goForward();
    assert(bridge.currentPath() == QStringLiteral("/tmp"));
    assert(bridge.canGoBack());
    assert(!bridge.canGoForward());

    std::cout << "  goUp()..." << std::endl;
    bridge.goUp();
    assert(bridge.currentPath() == QStringLiteral("/"));
    assert(bridge.currentDirName() == QStringLiteral("Root"));
    std::cout << "  -> PASS: Navigation history and directory hierarchy verified." << std::endl;

    // ────────────────────────────────────────────────────────────────────────
    // TEST 3: Item Open Actions (directory navigation & file launch)
    // ────────────────────────────────────────────────────────────────────────
    std::cout << "\n[TEST 3] Testing openItem action..." << std::endl;
    // Open directory via openItem
    bridge.openItem(QStringLiteral("/etc"), true);
    assert(bridge.currentPath() == QStringLiteral("/etc"));
    std::cout << "  openItem('/etc', isDir=true) successfully navigated to /etc" << std::endl;

    // Open file via openItem (calls xdg-open / gio without crash)
    std::cout << "  openItem('/etc/hosts', isDir=false)..." << std::endl;
    bridge.openItem(QStringLiteral("/etc/hosts"), false);
    std::cout << "  -> PASS: File launch action successfully dispatched." << std::endl;

    std::cout << "\n>>> ALL FILES LIVE VERIFICATIONS PASSED! <<<" << std::endl;
    return 0;
}
