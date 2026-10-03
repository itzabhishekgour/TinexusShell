// ============================================================================
// main.cpp — tinexus-about (Qt6)
// ============================================================================
#include "AboutBridge.hpp"
#include <common/logger.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("about");
    tinexus::log::info("Starting tinexus-about (Qt6)...");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-about"));
    app.setDesktopFileName(QStringLiteral("tinexus-about"));

    tinexus::about::AboutBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/AboutWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/about/qml/AboutWindow.qml"),
        QStringLiteral("src/about/qml/AboutWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/about/qml/AboutWindow.qml"),
        QStringLiteral("/usr/share/tinexus/about/qml/AboutWindow.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate AboutWindow.qml" << std::endl;
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object for about" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        window->show();
    }

    return app.exec();
}
