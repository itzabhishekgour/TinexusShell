// ============================================================================
// main.cpp — tinexus-monitor (Qt6 / QML)
// ============================================================================
#include "MonitorBridge.hpp"
#include <common/logger.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("monitor");
    tinexus::log::info("Starting tinexus-monitor (Qt6)...");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-monitor"));
    app.setDesktopFileName(QStringLiteral("tinexus-monitor"));

    tinexus::monitor::MonitorBridge monitorBridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("monitorBridge"), &monitorBridge);

    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/MonitorWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/monitor/qml/MonitorWindow.qml"),
        QStringLiteral("src/monitor/qml/MonitorWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/monitor/qml/MonitorWindow.qml"),
        QStringLiteral("/usr/share/tinexus/monitor/qml/MonitorWindow.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate MonitorWindow.qml" << std::endl;
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object for monitor" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        window->show();
    }

    return app.exec();
}
