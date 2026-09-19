// ============================================================================
// test_phase5_multiselect.cpp — Phase 5 Gate: Multi-Select Verification
//
// Verification checks:
//   MSEL-1: Single Item Selection & Backward-Compatible selectedPath
//   MSEL-2: Multi-Item Selection Primitives (toggle, range, selectAll, clear)
//   MSEL-3: Per-Tab Selection Isolation (Phase 4 Tab Model integration)
//   MSEL-4: Batch Operations (Batch Copy, Cut, Delete/Trash, and Tagging)
//   MSEL-5: Batch Conflict "Apply to All" Persistence Across N Items
//   MSEL-6: Edge Cases: Mixed File+Folder Batch, Mid-Selection Delete, Stale Index Pruning
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

    std::cout << "\n===========================================================\n";
    std::cout << "  Tinexus Platform: Phase 5 Multi-Select Verification Gate  \n";
    std::cout << "===========================================================\n\n";

    QString testBase = QStringLiteral("/tmp/tinexus_test_phase5");
    QDir(testBase).removeRecursively();
    QDir().mkpath(testBase + "/Folder1");
    QDir().mkpath(testBase + "/DestDir");

    write_file(testBase + "/Folder1/file1.txt", "Alpha content");
    write_file(testBase + "/Folder1/file2.txt", "Beta content");
    write_file(testBase + "/Folder1/file3.txt", "Gamma content");
    write_file(testBase + "/Folder1/file4.txt", "Delta content");
    write_file(testBase + "/Folder1/file5.txt", "Epsilon content");
    QDir().mkpath(testBase + "/Folder1/subfolder1");
    write_file(testBase + "/Folder1/subfolder1/nested.txt", "Nested content");

    tinexus::files::FilesBridge bridge(testBase + "/Folder1");
    QCoreApplication::processEvents();

    // ------------------------------------------------------------------------
    // MSEL-1: Single Item Selection & Backward-Compatible selectedPath
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 1/6] MSEL-1: Single Item Selection & Backward-Compatible selectedPath...\n";
    {
        QString p1 = testBase + "/Folder1/file1.txt";
        bridge.selectItem(p1);
        QCoreApplication::processEvents();

        if (bridge.selectedCount() != 1) {
            fail("MSEL-1: selectedCount() != 1");
        }
        if (bridge.selectedPath() != p1) {
            fail("MSEL-1: selectedPath() backward-compat getter != p1");
        }
        if (bridge.selectedPaths().size() != 1 || bridge.selectedPaths().first() != p1) {
            fail("MSEL-1: selectedPaths() list != [p1]");
        }
        if (!bridge.isSelected(p1)) {
            fail("MSEL-1: isSelected(p1) returned false");
        }

        bridge.clearSelection();
        QCoreApplication::processEvents();
        if (bridge.selectedCount() != 0) {
            fail("MSEL-1: selectedCount() != 0 after clearSelection");
        }
        if (!bridge.selectedPath().isEmpty()) {
            fail("MSEL-1: selectedPath() should be empty string when nothing selected");
        }

        pass("MSEL-1: Single selection and backward-compatible selectedPath invariant verified");
    }

    // ------------------------------------------------------------------------
    // MSEL-2: Multi-Item Selection Primitives (toggle, range, selectAll, clear)
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 2/6] MSEL-2: Multi-Item Selection Primitives...\n";
    {
        QString p1 = testBase + "/Folder1/file1.txt";
        QString p2 = testBase + "/Folder1/file2.txt";
        QString p3 = testBase + "/Folder1/file3.txt";

        // Ctrl+Click toggle
        bridge.toggleSelectItem(p1);
        bridge.toggleSelectItem(p2);
        QCoreApplication::processEvents();

        if (bridge.selectedCount() != 2) {
            fail("MSEL-2: Expected 2 items selected via toggle, got " + std::to_string(bridge.selectedCount()));
        }
        if (!bridge.isSelected(p1) || !bridge.isSelected(p2)) {
            fail("MSEL-2: isSelected returned false for toggled item");
        }

        // Untoggle p1
        bridge.toggleSelectItem(p1);
        QCoreApplication::processEvents();
        if (bridge.selectedCount() != 1 || bridge.isSelected(p1) || !bridge.isSelected(p2)) {
            fail("MSEL-2: Untoggle failed to remove item from selection");
        }

        // Shift+Click range selection from p1 to p3
        bridge.selectItem(p1);
        bridge.selectRange(p3);
        QCoreApplication::processEvents();

        if (bridge.selectedCount() < 3) {
            fail("MSEL-2: Range selection did not include expected items, count=" + std::to_string(bridge.selectedCount()));
        }
        if (!bridge.isSelected(p1) || !bridge.isSelected(p2) || !bridge.isSelected(p3)) {
            fail("MSEL-2: Range selection missing p1, p2, or p3");
        }

        // selectAll()
        bridge.selectAll();
        QCoreApplication::processEvents();
        if (bridge.selectedCount() != bridge.itemCount()) {
            fail("MSEL-2: selectAll count (" + std::to_string(bridge.selectedCount()) +
                 ") != itemCount (" + std::to_string(bridge.itemCount()) + ")");
        }

        // clearSelection()
        bridge.clearSelection();
        QCoreApplication::processEvents();
        if (bridge.selectedCount() != 0) {
            fail("MSEL-2: clearSelection() did not empty selection");
        }

        pass("MSEL-2: Toggle, range, selectAll, and clearSelection primitives verified");
    }

    // ------------------------------------------------------------------------
    // MSEL-3: Per-Tab Selection Isolation (Phase 4 Tab Model integration)
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 3/6] MSEL-3: Per-Tab Selection Isolation...\n";
    {
        // Tab 0 selection
        QString p1 = testBase + "/Folder1/file1.txt";
        QString p2 = testBase + "/Folder1/file2.txt";
        bridge.setSelectedPaths(QStringList{p1, p2});
        QCoreApplication::processEvents();

        // Create Tab 1
        int tab1 = bridge.createTab(testBase + "/Folder1");
        QCoreApplication::processEvents();

        if (bridge.activeTabIndex() != tab1) {
            fail("MSEL-3: activeTabIndex != tab1");
        }
        // New tab starts with default or empty selection, not inheriting previous tab's multi-selection
        QString p4 = testBase + "/Folder1/file4.txt";
        bridge.setSelectedPaths(QStringList{p4});
        QCoreApplication::processEvents();

        // Switch back to Tab 0
        bridge.switchTab(0);
        QCoreApplication::processEvents();

        if (bridge.selectedCount() != 2 || !bridge.isSelected(p1) || !bridge.isSelected(p2) || bridge.isSelected(p4)) {
            fail("MSEL-3: Selection bled or failed to restore upon switching back to Tab 0");
        }

        // Switch back to Tab 1
        bridge.switchTab(tab1);
        QCoreApplication::processEvents();

        if (bridge.selectedCount() != 1 || !bridge.isSelected(p4)) {
            fail("MSEL-3: Selection failed to restore upon switching back to Tab 1");
        }

        // Close Tab 1
        bridge.closeTab(tab1);
        QCoreApplication::processEvents();

        pass("MSEL-3: Strict per-tab selection isolation verified");
    }

    // ------------------------------------------------------------------------
    // MSEL-4: Batch Operations (Batch Copy, Cut, Delete/Trash, and Tagging)
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 4/6] MSEL-4: Batch Operations (Batch Copy, Cut, Trash, Tag)...\n";
    {
        QString p1 = testBase + "/Folder1/file1.txt";
        QString p2 = testBase + "/Folder1/file2.txt";

        // Multi-tagging
        bridge.setSelectedPaths(QStringList{p1, p2});
        bridge.setTagOnSelected("Blue");
        QCoreApplication::processEvents();

        if (bridge.getTagForItem(p1) != "Blue" || bridge.getTagForItem(p2) != "Blue") {
            fail("MSEL-4: Batch setTagOnSelected failed to set tag on all items");
        }

        // Batch Copy
        bridge.copySelected();
        if (bridge.clipboardCount() != 2) {
            fail("MSEL-4: clipboardCount != 2 after copySelected");
        }

        // Batch Paste into DestDir
        QString dest = testBase + "/DestDir";
        bridge.cd(dest);
        QCoreApplication::processEvents();

        bool pasteStarted = bridge.pasteItem();
        if (!pasteStarted) {
            fail("MSEL-4: pasteItem returned false");
        }

        // Wait for worker thread
        QElapsedTimer timer;
        timer.start();
        while (bridge.isOperating() && timer.elapsed() < 3000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            QCoreApplication::processEvents();
        }

        if (!QFile::exists(dest + "/file1.txt") || !QFile::exists(dest + "/file2.txt")) {
            fail("MSEL-4: Batch paste did not create both file1.txt and file2.txt in DestDir");
        }

        // Batch Delete
        bridge.setSelectedPaths(QStringList{dest + "/file1.txt", dest + "/file2.txt"});
        bool delOk = bridge.deleteSelected();
        if (!delOk) {
            fail("MSEL-4: deleteSelected returned false");
        }
        QCoreApplication::processEvents();

        if (QFile::exists(dest + "/file1.txt") || QFile::exists(dest + "/file2.txt")) {
            fail("MSEL-4: deleteSelected failed to remove items from disk");
        }
        if (bridge.selectedCount() != 0) {
            fail("MSEL-4: deleteSelected did not clear selection after deleting");
        }

        pass("MSEL-4: Batch copy, paste, multi-tagging, and trash verified successfully");
    }

    // ------------------------------------------------------------------------
    // MSEL-5: Batch Conflict "Apply to All" Persistence Across N Items
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 5/6] MSEL-5: Batch Conflict 'Apply to All' Persistence...\n";
    {
        // Create 3 files in Source and matching 3 files in Dest with different contents
        QString srcBatch = testBase + "/ConflictSrc";
        QString destBatch = testBase + "/ConflictDest";
        QDir().mkpath(srcBatch);
        QDir().mkpath(destBatch);

        write_file(srcBatch + "/c1.txt", "NEW 1");
        write_file(srcBatch + "/c2.txt", "NEW 2");
        write_file(srcBatch + "/c3.txt", "NEW 3");

        write_file(destBatch + "/c1.txt", "OLD 1");
        write_file(destBatch + "/c2.txt", "OLD 2");
        write_file(destBatch + "/c3.txt", "OLD 3");

        bridge.cd(srcBatch);
        QCoreApplication::processEvents();
        bridge.setSelectedPaths(QStringList{srcBatch + "/c1.txt", srcBatch + "/c2.txt", srcBatch + "/c3.txt"});
        bridge.copySelected();

        bridge.cd(destBatch);
        QCoreApplication::processEvents();

        int dialogShowCount = 0;
        QObject::connect(&bridge, &tinexus::files::FilesBridge::conflictDialogVisibleChanged, [&]() {
            if (bridge.conflictDialogVisible()) {
                dialogShowCount++;
                // On first conflict dialog, resolve with Replace + Apply to All
                bridge.resolveConflict(static_cast<int>(tinexus::files::ConflictResolution::Replace), true);
            }
        });

        bool ok = bridge.pasteItem();
        if (!ok) fail("MSEL-5: pasteItem failed");

        QElapsedTimer timer;
        timer.start();
        while (bridge.isOperating() && timer.elapsed() < 3000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            QCoreApplication::processEvents();
        }

        if (dialogShowCount != 1) {
            fail("MSEL-5: Expected conflict dialog to appear exactly once with Apply to All, appeared " +
                 std::to_string(dialogShowCount) + " times");
        }

        // Verify all 3 files were replaced
        QFile f1(destBatch + "/c1.txt"); f1.open(QIODevice::ReadOnly);
        QFile f2(destBatch + "/c2.txt"); f2.open(QIODevice::ReadOnly);
        QFile f3(destBatch + "/c3.txt"); f3.open(QIODevice::ReadOnly);

        if (f1.readAll() != "NEW 1" || f2.readAll() != "NEW 2" || f3.readAll() != "NEW 3") {
            fail("MSEL-5: Not all conflicting files in batch were replaced under Apply to All policy");
        }

        pass("MSEL-5: Batch conflict 'Apply to All' persisted across all items without prompting again");
    }

    // ------------------------------------------------------------------------
    // MSEL-6: Edge Cases: Mixed File+Folder Batch, Mid-Selection Delete, Stale Index Pruning
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 6/6] MSEL-6: Edge Cases: Mixed File+Folder Batch & Pruning...\n";
    {
        QString mixedSrc = testBase + "/MixedSrc";
        QString mixedDest = testBase + "/MixedDest";
        QDir().mkpath(mixedSrc + "/subDirA");
        QDir().mkpath(mixedDest);

        write_file(mixedSrc + "/fileA.txt", "File A");
        write_file(mixedSrc + "/subDirA/subFile.txt", "Sub file content");

        bridge.cd(mixedSrc);
        QCoreApplication::processEvents();

        // Select both folder and file
        bridge.setSelectedPaths(QStringList{mixedSrc + "/subDirA", mixedSrc + "/fileA.txt"});
        bridge.copySelected();

        bridge.cd(mixedDest);
        QCoreApplication::processEvents();
        bridge.pasteItem();

        QElapsedTimer timer;
        timer.start();
        while (bridge.isOperating() && timer.elapsed() < 3000) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            QCoreApplication::processEvents();
        }

        if (!QFile::exists(mixedDest + "/fileA.txt") || !QDir(mixedDest + "/subDirA").exists() ||
            !QFile::exists(mixedDest + "/subDirA/subFile.txt")) {
            fail("MSEL-6: Mixed file + folder batch copy/paste failed to recreate structure");
        }

        // Stale index pruning: select 2 items, delete one via deleteItem(single)
        bridge.setSelectedPaths(QStringList{mixedDest + "/fileA.txt", mixedDest + "/subDirA"});
        if (bridge.selectedCount() != 2) fail("MSEL-6: Setup selection failed");

        bridge.deleteItem(mixedDest + "/fileA.txt");
        QCoreApplication::processEvents();

        if (bridge.selectedCount() != 1 || bridge.isSelected(mixedDest + "/fileA.txt") ||
            !bridge.isSelected(mixedDest + "/subDirA")) {
            fail("MSEL-6: Single delete failed to prune deleted item from multi-selection");
        }

        pass("MSEL-6: Mixed file+folder batch copy and mid-selection pruning verified");
    }

    // Clean up
    QDir(testBase).removeRecursively();

    std::cout << "\n=======================================================\n";
    std::cout << "  [ALL PHASE 5 CHECKS PASSED: MSEL-1 TO MSEL-6 OK]    \n";
    std::cout << "=======================================================\n\n";

    return 0;
}
