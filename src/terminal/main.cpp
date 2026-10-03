#include "TerminalBridge.hpp"
#include "TerminalCanvas.hpp"
#include <common/logger.hpp>
#include <tinexus/client.hpp>

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <iostream>

using namespace tinexus::terminal;

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("terminal");
    tinexus::log::info("Starting Tinexus Terminal (Qt6/QML)...");

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-terminal"));
    app.setDesktopFileName(QStringLiteral("tinexus-terminal"));

    qmlRegisterType<TerminalBridge>("tinexus.terminal", 1, 0, "TerminalBridge");
    qmlRegisterType<TerminalCanvas>("tinexus.terminal", 1, 0, "TerminalCanvas");

    QQmlApplicationEngine engine;

    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/TerminalWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/terminal/qml/TerminalWindow.qml"),
        QStringLiteral("src/terminal/qml/TerminalWindow.qml"),
        QStringLiteral("/workspace/src/terminal/qml/TerminalWindow.qml"),
        QStringLiteral("/usr/share/tinexus/terminal/qml/TerminalWindow.qml")
    };
    for (const auto& cand : candidates) {
        if (QFileInfo::exists(cand)) {
            qmlPath = cand;
            break;
        }
    }

    if (qmlPath.isEmpty()) {
        std::cerr << "FAIL: Could not locate TerminalWindow.qml" << std::endl;
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object for terminal" << std::endl;
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        window->show();
    }

    return app.exec();
}
