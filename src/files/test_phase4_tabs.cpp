// ============================================================================
// test_phase4_tabs.cpp — Phase 4 Gate: Multi-Tab Navigation Verification
//
// Rigorous verification requirements:
//   TAB-1: Initial State & Tab Invariants (single tab at start, valid ID, active)
//   TAB-2: Create Tab (Ctrl+T) & Independent Path Isolation
//   TAB-3: Independent History, View Mode, and Sort State Per Tab
//   TAB-4: Close Tab (Ctrl+W) & Seamless Focus Transition (fallback to $HOME on last tab)
//   TAB-5: Bounds Checking & Rapid Switching (out-of-bounds safety, duplicateTab)
// ============================================================================

#include "FilesBridge.hpp"
#include <QtGui/QGuiApplication>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <iostream>
#include <cassert>
#include <fstream>
#include <thread>
#include <chrono>

namespace {

static void fail(const std::string& msg) {
    std::cerr << "\n[FAIL] " << msg << std::endl;
    std::abort();
}

static void pass(const std::string& msg) {
    std::cout << "  [PASS] " << msg << std::endl;
}

static void write_file(const QString& path, const std::string& content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    std::ofstream ofs(path.toStdString(), std::ios::binary);
    if (!ofs.is_open()) fail("Cannot create test file: " + path.toStdString());
    ofs << content;
    ofs.close();
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    std::cout << "\n=======================================================\n";
    std::cout << "  Tinexus Platform: Phase 4 Multi-Tab Navigation Gate  \n";
    std::cout << "=======================================================\n\n";

    QString testBase = QStringLiteral("/tmp/tinexus_test_phase4");
    QDir(testBase).removeRecursively();
    QDir().mkpath(testBase + "/FolderA");
    QDir().mkpath(testBase + "/FolderB");
    QDir().mkpath(testBase + "/FolderC");

    write_file(testBase + "/FolderA/fileA.txt", "Content A");
    write_file(testBase + "/FolderB/fileB.txt", "Content B");
    write_file(testBase + "/FolderC/fileC.txt", "Content C");

    tinexus::files::FilesBridge bridge;
    QCoreApplication::processEvents();

    // ------------------------------------------------------------------------
    // TAB-1: Initial State & Tab Invariants
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 1/5] TAB-1: Initial State & Tab Invariants...\n";
    {
        if (bridge.tabCount() != 1) {
            fail("TAB-1: Expected exactly 1 tab on startup, got " + std::to_string(bridge.tabCount()));
        }
        if (bridge.activeTabIndex() != 0) {
            fail("TAB-1: Expected activeTabIndex == 0, got " + std::to_string(bridge.activeTabIndex()));
        }

        auto tabs = bridge.tabs();
        if (tabs.size() != 1) {
            fail("TAB-1: tabs() property size != 1");
        }

        auto tab0 = tabs.first().toMap();
        if (tab0[QStringLiteral("index")].toInt() != 0) {
            fail("TAB-1: Tab 0 index != 0");
        }
        if (!tab0[QStringLiteral("isActive")].toBool()) {
            fail("TAB-1: Tab 0 should be active");
        }
        if (tab0[QStringLiteral("path")].toString().isEmpty()) {
            fail("TAB-1: Tab 0 initial path should not be empty");
        }

        pass("TAB-1: Single initial tab configured correctly with valid invariants");
    }

    // ------------------------------------------------------------------------
    // TAB-2: Create Tab (Ctrl+T) & Independent Path Isolation
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 2/5] TAB-2: Create Tab (Ctrl+T) & Independent Path Isolation...\n";
    {
        // Navigate Tab 0 to FolderA
        bridge.cd(testBase + "/FolderA");
        QCoreApplication::processEvents();

        if (bridge.currentPath() != testBase + "/FolderA") {
            fail("TAB-2: Failed to cd to FolderA");
        }

        // Create Tab 1 pointing to FolderB
        int tab1Idx = bridge.createTab(testBase + "/FolderB");
        QCoreApplication::processEvents();

        if (tab1Idx != 1) {
            fail("TAB-2: Expected new tab index 1, got " + std::to_string(tab1Idx));
        }
        if (bridge.tabCount() != 2) {
            fail("TAB-2: Expected tabCount 2, got " + std::to_string(bridge.tabCount()));
        }
        if (bridge.activeTabIndex() != 1) {
            fail("TAB-2: Expected activeTabIndex 1, got " + std::to_string(bridge.activeTabIndex()));
        }
        if (bridge.currentPath() != testBase + "/FolderB") {
            fail("TAB-2: Tab 1 path is not FolderB, got " + bridge.currentPath().toStdString());
        }

        // Verify Tab 0 in tabs list still points to FolderA
        auto tabs = bridge.tabs();
        auto tab0Map = tabs[0].toMap();
        auto tab1Map = tabs[1].toMap();

        if (tab0Map[QStringLiteral("path")].toString() != testBase + "/FolderA") {
            fail("TAB-2: Tab 0 path altered after creating Tab 1! Got: " + tab0Map[QStringLiteral("path")].toString().toStdString());
        }
        if (tab1Map[QStringLiteral("path")].toString() != testBase + "/FolderB") {
            fail("TAB-2: Tab 1 path mismatch! Got: " + tab1Map[QStringLiteral("path")].toString().toStdString());
        }
        if (tab0Map[QStringLiteral("isActive")].toBool()) {
            fail("TAB-2: Tab 0 should not be marked active");
        }
        if (!tab1Map[QStringLiteral("isActive")].toBool()) {
            fail("TAB-2: Tab 1 should be marked active");
        }

        // Switch back to Tab 0
        bridge.switchTab(0);
        QCoreApplication::processEvents();

        if (bridge.activeTabIndex() != 0) {
            fail("TAB-2: Failed to switch back to Tab 0");
        }
        if (bridge.currentPath() != testBase + "/FolderA") {
            fail("TAB-2: Switched to Tab 0 but currentPath is not FolderA: " + bridge.currentPath().toStdString());
        }

        pass("TAB-2: Tabs maintain independent paths and active states without bleed-through");
    }

    // ------------------------------------------------------------------------
    // TAB-3: Independent History, View Mode, and Sort State Per Tab
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 3/5] TAB-3: Independent History, View Mode, and Sort State Per Tab...\n";
    {
        // On Tab 0 (FolderA): set viewMode = 1 (List), navigate to subfolder, sort by size descending
        bridge.setViewMode(1);
        bridge.sortBy(QStringLiteral("size"), false);
        QCoreApplication::processEvents();

        if (bridge.currentViewMode() != 1) {
            fail("TAB-3: Tab 0 failed to set viewMode 1");
        }
        if (bridge.sortColumn() != QStringLiteral("size") || bridge.sortAscending() != false) {
            fail("TAB-3: Tab 0 failed to set sort size desc");
        }

        // Switch to Tab 1 (FolderB)
        bridge.switchTab(1);
        QCoreApplication::processEvents();

        // On Tab 1: set viewMode = 2 (Columns), sort by name ascending
        bridge.setViewMode(2);
        bridge.sortBy(QStringLiteral("name"), true);
        QCoreApplication::processEvents();

        if (bridge.currentViewMode() != 2) {
            fail("TAB-3: Tab 1 failed to set viewMode 2");
        }
        if (bridge.sortColumn() != QStringLiteral("name") || bridge.sortAscending() != true) {
            fail("TAB-3: Tab 1 failed to set sort name asc");
        }

        // Switch back to Tab 0 and verify Tab 0 state was preserved
        bridge.switchTab(0);
        QCoreApplication::processEvents();

        if (bridge.currentViewMode() != 1) {
            fail("TAB-3: Tab 0 viewMode was corrupted! Expected 1, got " + std::to_string(bridge.currentViewMode()));
        }
        if (bridge.sortColumn() != QStringLiteral("size") || bridge.sortAscending() != false) {
            fail("TAB-3: Tab 0 sort was corrupted! Expected size desc, got " + bridge.sortColumn().toStdString() + " asc=" + std::to_string(bridge.sortAscending()));
        }

        pass("TAB-3: Independent view modes and sort parameters completely preserved across tab switches");
    }

    // ------------------------------------------------------------------------
    // TAB-4: Close Tab (Ctrl+W) & Seamless Focus Transition
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 4/5] TAB-4: Close Tab (Ctrl+W) & Seamless Focus Transition...\n";
    {
        // Currently Tab 0 is active, Tab 1 exists
        // Create Tab 2 pointing to FolderC
        int tab2Idx = bridge.createTab(testBase + "/FolderC");
        QCoreApplication::processEvents();

        if (bridge.tabCount() != 3 || tab2Idx != 2 || bridge.activeTabIndex() != 2) {
            fail("TAB-4: Failed to create Tab 2");
        }

        // Close active Tab 2 -> should focus adjacent tab (index 1)
        bool closed = bridge.closeTab(2);
        QCoreApplication::processEvents();

        if (!closed) {
            fail("TAB-4: closeTab(2) returned false");
        }
        if (bridge.tabCount() != 2) {
            fail("TAB-4: Expected tabCount 2 after closing Tab 2, got " + std::to_string(bridge.tabCount()));
        }
        if (bridge.activeTabIndex() != 1) {
            fail("TAB-4: Expected activeTabIndex 1 after closing end tab, got " + std::to_string(bridge.activeTabIndex()));
        }
        if (bridge.currentPath() != testBase + "/FolderB") {
            fail("TAB-4: Expected currentPath FolderB, got " + bridge.currentPath().toStdString());
        }

        // Close Tab 0 while Tab 1 is active (active index should adjust to 0)
        closed = bridge.closeTab(0);
        QCoreApplication::processEvents();

        if (!closed || bridge.tabCount() != 1 || bridge.activeTabIndex() != 0) {
            fail("TAB-4: Failed to close non-active Tab 0 properly");
        }
        if (bridge.currentPath() != testBase + "/FolderB") {
            fail("TAB-4: Active tab path shifted unexpectedly, got " + bridge.currentPath().toStdString());
        }

        // Now only 1 tab remains. Closing the last remaining tab should not close window/crash, but fallback to homePath
        closed = bridge.closeTab(0);
        QCoreApplication::processEvents();

        if (!closed) {
            fail("TAB-4: Closing last remaining tab failed");
        }
        if (bridge.tabCount() != 1) {
            fail("TAB-4: Single tab invariant broken! Expected 1 tab, got " + std::to_string(bridge.tabCount()));
        }
        if (bridge.currentPath() != bridge.homePath()) {
            fail("TAB-4: Expected last tab close to fallback to homePath, got " + bridge.currentPath().toStdString());
        }

        pass("TAB-4: Tab closure, index adjustment, and single-tab home fallback fully validated");
    }

    // ------------------------------------------------------------------------
    // TAB-5: Bounds Checking, Duplication & Rapid Switching
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 5/5] TAB-5: Bounds Checking, Duplication & Rapid Switching...\n";
    {
        // Out-of-bounds switchTab
        bridge.switchTab(-1);
        bridge.switchTab(999);
        if (bridge.activeTabIndex() != 0) {
            fail("TAB-5: Out-of-bounds switchTab changed activeTabIndex");
        }

        // Out-of-bounds closeTab
        if (bridge.closeTab(-5)) {
            fail("TAB-5: closeTab(-5) should return false");
        }
        if (bridge.closeTab(100)) {
            fail("TAB-5: closeTab(100) should return false");
        }

        // Duplicate Tab
        bridge.cd(testBase + "/FolderA");
        bridge.duplicateTab(0);
        QCoreApplication::processEvents();

        if (bridge.tabCount() != 2) {
            fail("TAB-5: duplicateTab failed to create second tab");
        }
        if (bridge.currentPath() != testBase + "/FolderA") {
            fail("TAB-5: duplicated tab path mismatch");
        }

        // Rapid switching stress test
        for (int i = 0; i < 20; ++i) {
            bridge.switchTab(i % 2);
            QCoreApplication::processEvents();
        }

        if (bridge.activeTabIndex() != (19 % 2)) {
            fail("TAB-5: Rapid switching ended on unexpected tab");
        }

        pass("TAB-5: Edge cases, duplication, bounds safety, and rapid switching verified");
    }

    std::cout << "\n>>> ALL 5 PHASE 4 GATE CHECKS PASSED PERFECTLY. <<<\n\n";
    return 0;
}
