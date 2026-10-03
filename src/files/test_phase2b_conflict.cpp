// ============================================================================
// test_phase2b_conflict.cpp — Phase 2B Gate Verification
//
// Rigorous verification requirements:
//   CONF-1: Skip resolution leaves existing target untouched
//   CONF-2: Replace resolution overwrites destination with source SHA-256 match
//   CONF-3: Keep Both resolution preserves original and creates suffixed file
//   CONF-4: Apply to All in batch resolves 3 conflicts with strictly 1 dialog prompt
//   CONF-5: Cancel while worker is waiting on condition variable unblocks
//           cooperatively via stop_token without deadlock
//   CONF-6: Directory-over-directory merges transparently without silent wipe
//   CONF-7: Type mismatch (file vs directory) populates warning & item count
// ============================================================================

#include "FilesBridge.hpp"
#include <files/file_operations.hpp>
#include <QtGui/QGuiApplication>
#include <QtCore/QTimer>
#include <QtCore/QElapsedTimer>
#include <QtCore/QEventLoop>
#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>
#include <iostream>
#include <cassert>
#include <fstream>
#include <vector>

namespace {

static void fail(const std::string& msg) {
    std::cerr << "\n[FAIL] " << msg << std::endl;
    std::abort();
}

static void pass(const std::string& msg) {
    std::cout << "  [PASS] " << msg << std::endl;
}

static QString compute_sha256(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        fail("Cannot open file for SHA-256: " + path.toStdString());
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    char buffer[64 * 1024];
    while (!file.atEnd()) {
        qint64 read = file.read(buffer, sizeof(buffer));
        if (read > 0) {
            hash.addData(QByteArrayView(buffer, read));
        }
    }
    return QString::fromUtf8(hash.result().toHex());
}

static void write_file(const QString& path, const std::string& content) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    std::ofstream ofs(path.toStdString(), std::ios::binary);
    if (!ofs.is_open()) fail("Cannot create test file: " + path.toStdString());
    ofs << content;
    ofs.close();
}

static std::string read_file(const QString& path) {
    std::ifstream ifs(path.toStdString(), std::ios::binary);
    if (!ifs.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}

} // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("test-phase2b-conflict"));

    std::cout << "\n========================================================" << std::endl;
    std::cout << "    TINEXUS PHASE 2B: CONFLICT RESOLUTION VERIFICATION  " << std::endl;
    std::cout << "========================================================\n" << std::endl;

    const QString testDir = QStringLiteral("/tmp/tinexus_phase2b_test");
    QDir(testDir).removeRecursively();
    QDir().mkpath(testDir);

    // ─────────────────────────────────────────────────────────────────────────
    // CONF-1: Skip Resolution
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::cout << "[CONF-1] Testing Skip resolution..." << std::endl;
        const QString srcDir = testDir + QStringLiteral("/src1");
        const QString dstDir = testDir + QStringLiteral("/dst1");
        const QString srcFile = srcDir + QStringLiteral("/doc.txt");
        const QString dstFile = dstDir + QStringLiteral("/doc.txt");

        write_file(srcFile, "INCOMING_SOURCE_CONTENT_V2");
        write_file(dstFile, "ORIGINAL_DESTINATION_CONTENT_V1");

        tinexus::files::FilesBridge bridge(srcDir);
        bridge.copyItem(srcFile);

        QEventLoop loop;
        QObject::connect(&bridge, &tinexus::files::FilesBridge::conflictDialogVisibleChanged, [&]() {
            if (bridge.conflictDialogVisible()) {
                std::cout << "  Conflict detected for: " << bridge.conflictDetails()[QStringLiteral("fileName")].toString().toStdString() << std::endl;
                // Choose Skip (0)
                bridge.resolveConflict(0, false);
            }
        });

        QObject::connect(&bridge, &tinexus::files::FilesBridge::operationCompleted, [&](bool success, const QString&) {
            assert(success);
            loop.quit();
        });

        bridge.pasteItem(dstDir);
        loop.exec();

        // Verify destination file was NOT modified
        assert(read_file(dstFile) == "ORIGINAL_DESTINATION_CONTENT_V1");
        pass("CONF-1 Skip resolution leaves existing target untouched");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CONF-2: Replace Resolution (with SHA-256 check)
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::cout << "\n[CONF-2] Testing Replace resolution (bitwise overwrite)..." << std::endl;
        const QString srcDir = testDir + QStringLiteral("/src2");
        const QString dstDir = testDir + QStringLiteral("/dst2");
        const QString srcFile = srcDir + QStringLiteral("/data.bin");
        const QString dstFile = dstDir + QStringLiteral("/data.bin");

        write_file(srcFile, "NEW_REPLACED_DATA_SHA256_CHECK_ABC123");
        write_file(dstFile, "OLD_STALE_DATA_XYZ987");

        const QString srcSha = compute_sha256(srcFile);

        tinexus::files::FilesBridge bridge(srcDir);
        bridge.copyItem(srcFile);

        QEventLoop loop;
        QObject::connect(&bridge, &tinexus::files::FilesBridge::conflictDialogVisibleChanged, [&]() {
            if (bridge.conflictDialogVisible()) {
                // Choose Replace (1)
                bridge.resolveConflict(1, false);
            }
        });

        QObject::connect(&bridge, &tinexus::files::FilesBridge::operationCompleted, [&](bool success, const QString&) {
            assert(success);
            loop.quit();
        });

        bridge.pasteItem(dstDir);
        loop.exec();

        // Verify destination file now matches source exactly
        const QString dstSha = compute_sha256(dstFile);
        assert(dstSha == srcSha);
        assert(read_file(dstFile) == "NEW_REPLACED_DATA_SHA256_CHECK_ABC123");
        pass("CONF-2 Replace resolution overwrites destination with source SHA-256 match");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CONF-3: Keep Both Resolution
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::cout << "\n[CONF-3] Testing Keep Both resolution..." << std::endl;
        const QString srcDir = testDir + QStringLiteral("/src3");
        const QString dstDir = testDir + QStringLiteral("/dst3");
        const QString srcFile = srcDir + QStringLiteral("/photo.jpg");
        const QString dstFile = dstDir + QStringLiteral("/photo.jpg");

        write_file(srcFile, "PHOTO_TWO");
        write_file(dstFile, "PHOTO_ONE");

        tinexus::files::FilesBridge bridge(srcDir);
        bridge.copyItem(srcFile);

        QEventLoop loop;
        QObject::connect(&bridge, &tinexus::files::FilesBridge::conflictDialogVisibleChanged, [&]() {
            if (bridge.conflictDialogVisible()) {
                // Choose Keep Both (2)
                bridge.resolveConflict(2, false);
            }
        });

        QObject::connect(&bridge, &tinexus::files::FilesBridge::operationCompleted, [&](bool success, const QString&) {
            assert(success);
            loop.quit();
        });

        bridge.pasteItem(dstDir);
        loop.exec();

        // Verify original file preserved AND suffixed file created
        assert(read_file(dstFile) == "PHOTO_ONE");
        const QString suffixedFile = dstDir + QStringLiteral("/photo (1).jpg");
        assert(QFile::exists(suffixedFile));
        assert(read_file(suffixedFile) == "PHOTO_TWO");
        pass("CONF-3 Keep Both resolution preserves original and creates suffixed file");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CONF-4: Apply to All in Batch (Strict single-invocation assertion)
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::cout << "\n[CONF-4] Testing Apply to All batch resolution (Strict single-prompt assertion)..." << std::endl;
        const QString srcDir = testDir + QStringLiteral("/src4/batch_dir");
        const QString dstDir = testDir + QStringLiteral("/dst4/batch_dir");

        write_file(srcDir + QStringLiteral("/file1.txt"), "SOURCE_1");
        write_file(srcDir + QStringLiteral("/file2.txt"), "SOURCE_2");
        write_file(srcDir + QStringLiteral("/file3.txt"), "SOURCE_3");

        write_file(dstDir + QStringLiteral("/file1.txt"), "ORIG_1");
        write_file(dstDir + QStringLiteral("/file2.txt"), "ORIG_2");
        write_file(dstDir + QStringLiteral("/file3.txt"), "ORIG_3");

        tinexus::files::FilesBridge bridge(srcDir);
        bridge.copyItem(srcDir); // Copy whole directory

        int promptCount = 0;
        QEventLoop loop;
        QObject::connect(&bridge, &tinexus::files::FilesBridge::conflictDialogVisibleChanged, [&]() {
            if (bridge.conflictDialogVisible()) {
                promptCount++;
                std::cout << "  Prompt #" << promptCount << " for: "
                          << bridge.conflictDetails()[QStringLiteral("fileName")].toString().toStdString() << std::endl;
                // Choose Replace (1) with applyToAll = true
                bridge.resolveConflict(1, true);
            }
        });

        QObject::connect(&bridge, &tinexus::files::FilesBridge::operationCompleted, [&](bool success, const QString&) {
            assert(success);
            loop.quit();
        });

        bridge.pasteItem(testDir + QStringLiteral("/dst4"));
        loop.exec();

        std::cout << "  Total conflict prompts triggered: " << promptCount << std::endl;
        if (promptCount != 1) {
            fail("Apply to all FAILED: prompt count was " + std::to_string(promptCount) + ", expected strictly 1!");
        }

        // Verify all 3 files in destination directory were replaced
        assert(read_file(dstDir + QStringLiteral("/file1.txt")) == "SOURCE_1");
        assert(read_file(dstDir + QStringLiteral("/file2.txt")) == "SOURCE_2");
        assert(read_file(dstDir + QStringLiteral("/file3.txt")) == "SOURCE_3");

        pass("CONF-4 Apply to All resolved 3 conflicts with strictly 1 dialog prompt");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CONF-5: Cancel While Waiting on Condition Variable (Deadlock Check)
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::cout << "\n[CONF-5] Testing cancel during condition variable wait (Deadlock check)..." << std::endl;
        const QString srcDir = testDir + QStringLiteral("/src5");
        const QString dstDir = testDir + QStringLiteral("/dst5");
        const QString srcFile = srcDir + QStringLiteral("/blocker.txt");
        const QString dstFile = dstDir + QStringLiteral("/blocker.txt");

        write_file(srcFile, "SRC_BLOCK");
        write_file(dstFile, "DST_BLOCK");

        tinexus::files::FilesBridge bridge(srcDir);
        bridge.copyItem(srcFile);

        QEventLoop loop;
        QElapsedTimer cancelTimer;
        bool promptSeen = false;

        QObject::connect(&bridge, &tinexus::files::FilesBridge::conflictDialogVisibleChanged, [&]() {
            if (bridge.conflictDialogVisible() && !promptSeen) {
                promptSeen = true;
                std::cout << "  Worker paused on condition variable. Triggering cancelCurrentOperation()..." << std::endl;
                cancelTimer.start();
                // Call cancel while worker is in cv.wait()
                bridge.cancelCurrentOperation();
                qint64 cancelElapsedMs = cancelTimer.elapsed();
                std::cout << "  cancelCurrentOperation() returned in " << cancelElapsedMs << " ms (Zero Deadlock!)" << std::endl;
                assert(cancelElapsedMs < 500); // Must join within 500ms
                loop.quit();
            }
        });

        bridge.pasteItem(dstDir);
        loop.exec();

        assert(promptSeen);
        assert(!bridge.isOperating());
        assert(!bridge.conflictDialogVisible());
        pass("CONF-5 Cancel while waiting on condition variable unblocks and joins thread cleanly without deadlock");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CONF-6: Directory Transparent Merge
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::cout << "\n[CONF-6] Testing Directory Transparent Merge..." << std::endl;
        const QString srcDir = testDir + QStringLiteral("/src6/MergeDir");
        const QString dstDir = testDir + QStringLiteral("/dst6/MergeDir");

        write_file(srcDir + QStringLiteral("/new_file.txt"), "NEW_CONTENT");
        write_file(dstDir + QStringLiteral("/existing_file.txt"), "EXISTING_CONTENT");

        tinexus::files::FilesBridge bridge(testDir + QStringLiteral("/src6"));
        bridge.copyItem(srcDir);

        QEventLoop loop;
        QObject::connect(&bridge, &tinexus::files::FilesBridge::operationCompleted, [&](bool success, const QString&) {
            assert(success);
            loop.quit();
        });

        bridge.pasteItem(testDir + QStringLiteral("/dst6"));
        loop.exec();

        // Verify existing file is still there and new file was copied inside
        assert(QFile::exists(dstDir + QStringLiteral("/existing_file.txt")));
        assert(read_file(dstDir + QStringLiteral("/existing_file.txt")) == "EXISTING_CONTENT");
        assert(QFile::exists(dstDir + QStringLiteral("/new_file.txt")));
        assert(read_file(dstDir + QStringLiteral("/new_file.txt")) == "NEW_CONTENT");
        pass("CONF-6 Directory-over-directory merges transparently without silent wipe");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CONF-7: Type Mismatch Safeguard (File vs Directory warning)
    // ─────────────────────────────────────────────────────────────────────────
    {
        std::cout << "\n[CONF-7] Testing Type Mismatch Safeguard..." << std::endl;
        const QString srcDir = testDir + QStringLiteral("/src7");
        const QString dstParent = testDir + QStringLiteral("/dst7");
        const QString clashItem = QStringLiteral("folder_or_file");

        // Source is a file
        write_file(srcDir + "/" + clashItem, "I_AM_A_FILE");

        // Destination has a directory with 3 files
        const QString dstDir = dstParent + "/" + clashItem;
        write_file(dstDir + QStringLiteral("/sub1.dat"), "A");
        write_file(dstDir + QStringLiteral("/sub2.dat"), "B");
        write_file(dstDir + QStringLiteral("/sub3.dat"), "C");

        tinexus::files::FilesBridge bridge(srcDir);
        bridge.copyItem(srcDir + "/" + clashItem);

        bool mismatchDetected = false;
        int reportedCount = 0;

        QEventLoop loop;
        QObject::connect(&bridge, &tinexus::files::FilesBridge::conflictDialogVisibleChanged, [&]() {
            if (bridge.conflictDialogVisible()) {
                auto details = bridge.conflictDetails();
                mismatchDetected = details[QStringLiteral("isTypeMismatch")].toBool();
                reportedCount = details[QStringLiteral("destDirItemCount")].toInt();
                std::cout << "  Type mismatch detected=" << mismatchDetected
                          << ", reported items inside folder=" << reportedCount << std::endl;
                // Choose Skip
                bridge.resolveConflict(0, false);
            }
        });

        QObject::connect(&bridge, &tinexus::files::FilesBridge::operationCompleted, [&](bool success, const QString&) {
            assert(success);
            loop.quit();
        });

        bridge.pasteItem(dstParent);
        loop.exec();

        assert(mismatchDetected);
        assert(reportedCount == 3);
        pass("CONF-7 Type mismatch correctly flagged with directory item count warning");
    }

    // ─────────────────────────────────────────────────────────────────────────
    // CLEANUP
    // ─────────────────────────────────────────────────────────────────────────
    QDir(testDir).removeRecursively();

    std::cout << "\n========================================================" << std::endl;
    std::cout << "    >>> ALL PHASE 2B CONFLICT GATE CHECKS PASSED <<<    " << std::endl;
    std::cout << "========================================================\n" << std::endl;
    return 0;
}
