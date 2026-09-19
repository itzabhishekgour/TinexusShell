// ============================================================================
// test_phase3_search.cpp — Phase 3 Gate Verification: Search & Address Bar
//
// Rigorous verification requirements:
//   SRCH-1: Exact & Substring Search Match
//   SRCH-2: Recursive Deep-Tree Search & Relative Path Mapping
//   SRCH-3: Rapid Query Debounce & Cooperative Cancellation
//   SRCH-4: Address Bar Tilde (~), Relative, and Error Path Navigation
//   SRCH-5: View Transparency & Seamless Model Swapping
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

static void wait_for_search(tinexus::files::FilesBridge& bridge, int timeoutMs = 3000) {
    QElapsedTimer timer;
    timer.start();
    while (!bridge.isSearchFinished() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    // Process any remaining invokeMethod calls
    for (int i = 0; i < 5; ++i) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    std::cout << "\n=======================================================\n";
    std::cout << "  Tinexus Platform: Phase 3 Search & Address Bar Gate  \n";
    std::cout << "=======================================================\n\n";

    QString testBase = QStringLiteral("/tmp/tinexus_test_phase3");
    QDir(testBase).removeRecursively();
    QDir().mkpath(testBase);

    // Populate test files
    write_file(testBase + "/report_2026.pdf", "Annual Financial Report 2026");
    write_file(testBase + "/finance_q3.xlsx", "Q3 Revenue Data");
    write_file(testBase + "/notes.txt", "Engineering meeting notes");
    write_file(testBase + "/Level1/Level2/Level3/deep_secret.txt", "Top secret recursive payload");
    write_file(testBase + "/Level1/other.txt", "Other file in Level1");

    tinexus::files::FilesBridge bridge;
    bridge.cd(testBase);
    QCoreApplication::processEvents();

    // ------------------------------------------------------------------------
    // SRCH-1: Exact & Substring Search Match
    // ------------------------------------------------------------------------
    std::cout << "[CHECK 1/5] SRCH-1: Exact & Substring Search Match...\n";
    {
        bridge.startSearch(QStringLiteral("report"), false);
        wait_for_search(bridge);

        if (bridge.itemCount() != 1) {
            fail("SRCH-1: Expected 1 match for 'report', got " + std::to_string(bridge.itemCount()));
        }
        auto results = bridge.fileList();
        QString name = results.first().toMap()[QStringLiteral("name")].toString();
        if (name != QStringLiteral("report_2026.pdf")) {
            fail("SRCH-1: Expected report_2026.pdf, got " + name.toStdString());
        }

        // Substring query 'q3'
        bridge.startSearch(QStringLiteral("q3"), false);
        wait_for_search(bridge);

        if (bridge.itemCount() != 1) {
            fail("SRCH-1: Expected 1 match for 'q3', got " + std::to_string(bridge.itemCount()));
        }
        results = bridge.fileList();
        name = results.first().toMap()[QStringLiteral("name")].toString();
        if (name != QStringLiteral("finance_q3.xlsx")) {
            fail("SRCH-1: Expected finance_q3.xlsx, got " + name.toStdString());
        }

        pass("SRCH-1: Substring search correctly matched target files");
    }

    // ------------------------------------------------------------------------
    // SRCH-2: Recursive Deep-Tree Search & Relative Path
    // ------------------------------------------------------------------------
    std::cout << "\n[CHECK 2/5] SRCH-2: Recursive Deep-Tree Search & Relative Path...\n";
    {
        bridge.startSearch(QStringLiteral("secret"), true);
        wait_for_search(bridge);

        if (bridge.itemCount() != 1) {
            fail("SRCH-2: Expected 1 match for recursive 'secret', got " + std::to_string(bridge.itemCount()));
        }

        auto results = bridge.fileList();
        auto item = results.first().toMap();
        QString name = item[QStringLiteral("name")].toString();
        QString relPath = item[QStringLiteral("relativePath")].toString();

        if (name != QStringLiteral("deep_secret.txt")) {
            fail("SRCH-2: Expected deep_secret.txt, got " + name.toStdString());
        }

        // Path separators could be / or \ depending on OS, normalize to /
        QString normRelPath = relPath;
        normRelPath.replace('\\', '/');

        if (!normRelPath.contains(QStringLiteral("Level1/Level2/Level3/deep_secret.txt"))) {
            fail("SRCH-2: Relative path incorrect: " + normRelPath.toStdString());
        }

        pass("SRCH-2: Recursive deep-tree search discovered nested file with accurate relative path");
    }

    // ------------------------------------------------------------------------
    // SRCH-3: Rapid Query Debounce & Cooperative Cancellation
    // ------------------------------------------------------------------------
    std::cout << "\n[CHECK 3/5] SRCH-3: Rapid Query Debounce & Cooperative Cancellation...\n";
    {
        // Fire 5 rapid queries in sequence without waiting between them
        bridge.startSearch(QStringLiteral("a"));
        bridge.startSearch(QStringLiteral("ab"));
        bridge.startSearch(QStringLiteral("abc"));
        bridge.startSearch(QStringLiteral("abcd"));
        bridge.startSearch(QStringLiteral("finance"));

        wait_for_search(bridge);

        if (bridge.searchQuery() != QStringLiteral("finance")) {
            fail("SRCH-3: Query mismatch, expected 'finance', got: " + bridge.searchQuery().toStdString());
        }

        if (bridge.itemCount() != 1) {
            fail("SRCH-3: Expected exactly 1 match for final query 'finance', got " + std::to_string(bridge.itemCount()));
        }

        auto results = bridge.fileList();
        QString name = results.first().toMap()[QStringLiteral("name")].toString();
        if (name != QStringLiteral("finance_q3.xlsx")) {
            fail("SRCH-3: Expected finance_q3.xlsx, got: " + name.toStdString());
        }

        pass("SRCH-3: Rapid queries cooperatively cancelled without deadlock or stale results");
    }

    // ------------------------------------------------------------------------
    // SRCH-4: Address Bar Tilde (~), Relative, and Error Path Navigation
    // ------------------------------------------------------------------------
    std::cout << "\n[CHECK 4/5] SRCH-4: Address Bar Tilde (~), Relative, and Error Path Navigation...\n";
    {
        // 1. Tilde navigation to ~
        bool ok = bridge.navigateToPath(QStringLiteral("~"));
        if (!ok || bridge.currentPath() != bridge.homePath()) {
            fail("SRCH-4: Failed navigating to '~'. Expected: " + bridge.homePath().toStdString() + " got: " + bridge.currentPath().toStdString());
        }

        // 2. Tilde navigation to ~/Documents
        ok = bridge.navigateToPath(QStringLiteral("~/Documents"));
        QString expectedDocs = QDir::cleanPath(bridge.homePath() + QStringLiteral("/Documents"));
        if (!ok || bridge.currentPath() != expectedDocs) {
            fail("SRCH-4: Failed navigating to '~/Documents'. Expected: " + expectedDocs.toStdString() + " got: " + bridge.currentPath().toStdString());
        }

        // 3. Relative navigation '..'
        ok = bridge.navigateToPath(QStringLiteral(".."));
        if (!ok || bridge.currentPath() != bridge.homePath()) {
            fail("SRCH-4: Failed navigating to '..'. Expected: " + bridge.homePath().toStdString() + " got: " + bridge.currentPath().toStdString());
        }

        // 4. Invalid path returns false and does not alter current path
        QString beforeInvalid = bridge.currentPath();
        ok = bridge.navigateToPath(QStringLiteral("/path/that/does/not/exist/at/all_123456789"));
        if (ok) {
            fail("SRCH-4: Invalid path navigation unexpectedly returned true");
        }
        if (bridge.currentPath() != beforeInvalid) {
            fail("SRCH-4: Current path was modified on invalid navigation!");
        }

        pass("SRCH-4: Tilde (~), relative, and invalid path handling validated");
    }

    // ------------------------------------------------------------------------
    // SRCH-5: View Transparency & Seamless Model Swapping
    // ------------------------------------------------------------------------
    std::cout << "\n[CHECK 5/5] SRCH-5: View Transparency & Seamless Model Swapping...\n";
    {
        bridge.cd(testBase);
        QCoreApplication::processEvents();

        int normalCount = bridge.itemCount();
        if (normalCount < 4) {
            fail("SRCH-5: Expected at least 4 items in testBase, got " + std::to_string(normalCount));
        }

        // Trigger search
        bridge.startSearch(QStringLiteral("report"), false);
        wait_for_search(bridge);

        if (!bridge.isSearching()) {
            fail("SRCH-5: isSearching should be true during search");
        }
        if (bridge.itemCount() != 1) {
            fail("SRCH-5: bridge.itemCount() during search should return 1, got " + std::to_string(bridge.itemCount()));
        }
        if (bridge.fileList().size() != 1) {
            fail("SRCH-5: bridge.fileList().size() should return 1, got " + std::to_string(bridge.fileList().size()));
        }

        // Clear search
        bridge.clearSearch();
        QCoreApplication::processEvents();

        if (bridge.isSearching()) {
            fail("SRCH-5: isSearching should be false after clearSearch()");
        }
        if (bridge.itemCount() != normalCount) {
            fail("SRCH-5: bridge.itemCount() should be restored to " + std::to_string(normalCount) + ", got " + std::to_string(bridge.itemCount()));
        }
        if (bridge.fileList().size() != normalCount) {
            fail("SRCH-5: bridge.fileList().size() should be restored to " + std::to_string(normalCount) + ", got " + std::to_string(bridge.fileList().size()));
        }

        pass("SRCH-5: View transparency confirmed: fileList() seamlessly toggles between search results and full directory");
    }

    // Cleanup
    QDir(testBase).removeRecursively();

    std::cout << "\n>>> ALL 5 PHASE 3 GATE CHECKS PASSED PERFECTLY. <<<\n\n";
    return 0;
}
