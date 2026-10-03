// ============================================================================
// main.cpp — tinexus-files (Qt6)
// ============================================================================
#include "FilesBridge.hpp"
#include <common/logger.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtCore/QTimer>
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("files");
    tinexus::log::info("Starting tinexus-files (Qt6)...");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-files"));
    app.setDesktopFileName(QStringLiteral("tinexus-files"));

    QString targetPath;
    if (argc > 1) {
        targetPath = QString::fromUtf8(argv[1]);
    }

    tinexus::files::FilesBridge bridge(targetPath);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    if (qEnvironmentVariableIsSet("TINEXUS_FILES_QML")) {
        qmlPath = qEnvironmentVariable("TINEXUS_FILES_QML");
    } else {
        const QString appDir = QCoreApplication::applicationDirPath();
        const QStringList candidates = {
            // Production install targets
            appDir + QStringLiteral("/qml/FilesWindow.qml"),
            appDir + QStringLiteral("/../share/tinexus/files/qml/FilesWindow.qml"),
            QStringLiteral("/usr/share/tinexus/files/qml/FilesWindow.qml"),
#if !defined(NDEBUG) || defined(TINEXUS_DEV_BUILD)
            // Development worktree fallback only
            QStringLiteral("/workspace/src/files/qml/FilesWindow.qml"),
            appDir + QStringLiteral("/../src/files/qml/FilesWindow.qml"),
            QStringLiteral("src/files/qml/FilesWindow.qml"),
            QStringLiteral("/mnt/host/e/Tinu's Technology/Tinexus Manager/src/files/qml/FilesWindow.qml"),
#endif
        };
        for (const auto& cand : candidates) {
            if (QFileInfo::exists(cand)) {
                qmlPath = cand;
                break;
            }
        }
    }

    if (qmlPath.isEmpty() || !QFileInfo::exists(qmlPath)) {
        tinexus::log::error("[files] Fatal: Could not locate FilesWindow.qml! Set TINEXUS_FILES_QML");
        std::cerr << "FAIL: Could not locate FilesWindow.qml" << std::endl;
        return 1;
    }

    tinexus::log::info("[files] Loading QML from: {}", qmlPath.toStdString());

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object for files" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        window->show();
    }

    // Automated Test Hooks (Triggered only if specific test env vars are set)
    if (qEnvironmentVariableIsSet("TINEXUS_TRIGGER_TEST_CONFLICT")) {
        QTimer::singleShot(1000, [&bridge, targetPath]() {
            QString testSrc = targetPath + QStringLiteral("/report_q3.pdf");
            bridge.copyItem(testSrc);
            bridge.pasteItem(targetPath); // Causes real collision with existing report_q3.pdf!
        });
    }
    if (qEnvironmentVariableIsSet("TINEXUS_TRIGGER_TEST_SEARCH")) {
        QTimer::singleShot(800, [&bridge]() {
            bridge.startSearch(QStringLiteral("report"), true);
        });
    }
    if (qEnvironmentVariableIsSet("TINEXUS_TRIGGER_TEST_TABS")) {
        tinexus::log::info("[files] Test hook: TINEXUS_TRIGGER_TEST_TABS triggered");
        QTimer::singleShot(600, [&bridge]() {
            tinexus::log::info("[files] Test hook: creating tabs Documents & Projects");
            bridge.createTab(QStringLiteral("/tmp/test_tabs_view/Documents"));
            bridge.createTab(QStringLiteral("/tmp/test_tabs_view/Projects"));
            bridge.switchTab(1); // Set active to Documents
        });
    }

    return app.exec();
}
