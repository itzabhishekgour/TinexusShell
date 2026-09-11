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
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/FilesWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/files/qml/FilesWindow.qml"),
        QStringLiteral("src/files/qml/FilesWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/files/qml/FilesWindow.qml"),
        QStringLiteral("/usr/share/tinexus/files/qml/FilesWindow.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate FilesWindow.qml" << std::endl;
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object for files" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        window->show();
    }

    return app.exec();
}
