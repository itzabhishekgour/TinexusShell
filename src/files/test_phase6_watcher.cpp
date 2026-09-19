// ============================================================================
// test_phase6_watcher.cpp — Phase 6 Gate: inotify Live File Watcher Verification
//
// Verification checks:
//   WATCH-1: Watch Descriptor Initialization on Active Directory
//   WATCH-2: Live File Creation Detection (touch creates entry without manual refresh)
//   WATCH-3: Live File Deletion Detection & Selection Pruning (rm removes entry)
//   WATCH-4: Live File Modification Detection (incremental size/mtime update)
//   WATCH-5: Directory Navigation Re-anchors inotify Watch
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

static void append_file(const QString& path, const std::string& content) {
    std::ofstream ofs(path.toStdString(), std::ios::binary | std::ios::app);
    if (!ofs.is_open()) fail("Cannot append to test file: " + path.toStdString());
    ofs << content;
    ofs.close();
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    std::cout << "\n===========================================================\n";
    std::cout << "  Tinexus Platform: Phase 6 inotify Live Watcher Gate       \n";
    std::cout << "===========================================================\n\n";

    QString testBase = QStringLiteral("/tmp/tinexus_test_phase6");
    QDir(testBase).removeRecursively();
    QDir().mkpath(testBase + "/WatchDirA");
    QDir().mkpath(testBase + "/WatchDirB");

    write_file(testBase + "/WatchDirA/init.txt", "Initial file");

    tinexus::files::FilesBridge bridge(testBase + "/WatchDirA");
    QCoreApplication::processEvents();

    // ------------------------------------------------------------------------
    // WATCH-1: Watch Initialization
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 1/5] WATCH-1: inotify Watch Initialization...\n";
    {
        if (bridge.currentPath() != QDir(testBase + "/WatchDirA").canonicalPath()) {
            fail("WATCH-1: Current path mismatch");
        }
        if (bridge.itemCount() != 1) {
            fail("WATCH-1: Expected 1 initial item, got " + std::to_string(bridge.itemCount()));
        }
        pass("WATCH-1: inotify live file watcher initialized on starting directory");
    }

    // ------------------------------------------------------------------------
    // WATCH-2: Live File Creation Detection (touch without manual refresh)
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 2/5] WATCH-2: Live File Creation Detection (touch)...\n";
    {
        QString newFile = testBase + "/WatchDirA/external_created.txt";
        write_file(newFile, "Created externally by background process");

        // Wait up to 2 seconds for inotify QSocketNotifier to fire and update model
        QElapsedTimer timer;
        timer.start();
        bool found = false;
        while (timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            for (const auto& item : bridge.fileList()) {
                if (item.toMap()[QStringLiteral("name")].toString() == "external_created.txt") {
                    found = true;
                    break;
                }
            }
            if (found) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (!found) {
            fail("WATCH-2: Live file creation was not detected by inotify without manual refresh");
        }
        pass("WATCH-2: External file creation detected instantly via inotify socket notifier");
    }

    // ------------------------------------------------------------------------
    // WATCH-3: Live File Deletion Detection & Selection Pruning (rm)
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 3/5] WATCH-3: Live File Deletion Detection & Selection Pruning...\n";
    {
        QString toDelete = testBase + "/WatchDirA/external_created.txt";
        bridge.selectItem(toDelete);
        QCoreApplication::processEvents();

        if (!bridge.isSelected(toDelete)) {
            fail("WATCH-3: Setup failed to select target file");
        }

        // Externally unlink file
        QFile::remove(toDelete);

        QElapsedTimer timer;
        timer.start();
        bool gone = false;
        while (timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            bool stillPresent = false;
            for (const auto& item : bridge.fileList()) {
                if (item.toMap()[QStringLiteral("name")].toString() == "external_created.txt") {
                    stillPresent = true;
                    break;
                }
            }
            if (!stillPresent) {
                gone = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (!gone) {
            fail("WATCH-3: Deleted file still present in model after external unlink");
        }
        if (bridge.isSelected(toDelete)) {
            fail("WATCH-3: Deleted file was not pruned from selectedPaths");
        }

        pass("WATCH-3: External file deletion detected and removed from model and selection");
    }

    // ------------------------------------------------------------------------
    // WATCH-4: Live File Modification Detection (incremental size update)
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 4/5] WATCH-4: Live File Modification Detection...\n";
    {
        QString initFile = testBase + "/WatchDirA/init.txt";
        // Append 2048 bytes of content to change size
        std::string extra(2048, 'X');
        append_file(initFile, extra);

        QElapsedTimer timer;
        timer.start();
        bool sizeUpdated = false;
        while (timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            for (const auto& item : bridge.fileList()) {
                if (item.toMap()[QStringLiteral("name")].toString() == "init.txt") {
                    QString sz = item.toMap()[QStringLiteral("sizeStr")].toString();
                    if (sz.contains("KB") || sz.contains("20")) {
                        sizeUpdated = true;
                        break;
                    }
                }
            }
            if (sizeUpdated) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (!sizeUpdated) {
            fail("WATCH-4: File modification did not update item sizeStr incrementally");
        }

        pass("WATCH-4: External file modification updated metadata incrementally");
    }

    // ------------------------------------------------------------------------
    // WATCH-5: Directory Navigation Re-anchors inotify Watch
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 5/5] WATCH-5: Directory Navigation Re-anchors inotify Watch...\n";
    {
        bridge.cd(testBase + "/WatchDirB");
        QCoreApplication::processEvents();

        // Write a file in WatchDirB
        QString bFile = testBase + "/WatchDirB/b_file.txt";
        write_file(bFile, "File in B");

        QElapsedTimer timer;
        timer.start();
        bool foundB = false;
        while (timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            for (const auto& item : bridge.fileList()) {
                if (item.toMap()[QStringLiteral("name")].toString() == "b_file.txt") {
                    foundB = true;
                    break;
                }
            }
            if (foundB) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        if (!foundB) {
            fail("WATCH-5: After navigation to WatchDirB, live creation was not detected");
        }

        // Write a file in old WatchDirA — should NOT appear in WatchDirB
        QString aGhost = testBase + "/WatchDirA/ghost.txt";
        write_file(aGhost, "Ghost in A");
        QCoreApplication::processEvents();

        for (const auto& item : bridge.fileList()) {
            if (item.toMap()[QStringLiteral("name")].toString() == "ghost.txt") {
                fail("WATCH-5: Ghost file from previous directory appeared in active view");
            }
        }

        pass("WATCH-5: inotify watch cleanly re-anchored on directory switch without leak");
    }

    // Clean up
    QDir(testBase).removeRecursively();

    std::cout << "\n=======================================================\n";
    std::cout << "  [ALL PHASE 6 CHECKS PASSED: WATCH-1 TO WATCH-5 OK]  \n";
    std::cout << "=======================================================\n\n";

    return 0;
}
