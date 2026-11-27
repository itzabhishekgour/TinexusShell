// ============================================================================
// main.cpp — tinexus-monitor (Qt6 / QML)
// ============================================================================
#include "MonitorBridge.hpp"
#include <common/logger.hpp>
#include <common/ui/TinexusAppIconProvider.hpp>
#include <common/ui/WaylandBlurHelper.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <algorithm>
#include <iostream>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("monitor");
    tinexus::log::info("Starting tinexus-monitor (Qt6)...");

    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "wayland");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-monitor"));
    app.setDesktopFileName(QStringLiteral("tinexus-monitor"));

    tinexus::monitor::MonitorBridge monitorBridge;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("tinexus"), new tinexus::common::ui::AppIconProvider());
    engine.rootContext()->setContextProperty(QStringLiteral("monitorBridge"), &monitorBridge);

    QString qmlPath;
    const QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/qml/MonitorWindow.qml"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/../src/monitor/qml/MonitorWindow.qml"),
        QStringLiteral("src/monitor/qml/MonitorWindow.qml"),
        QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/monitor/qml/MonitorWindow.qml"),
        QStringLiteral("/usr/share/tinexus/monitor/qml/MonitorWindow.qml"),
        QStringLiteral("/usr/share/tinexus-monitor/qml/MonitorWindow.qml")
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

    tinexus::common::ui::WaylandBlurHelper blurHelper;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        auto updateWindowBlur = [window, &blurHelper]() {
            if (!window) return;
            const int w = std::max(1, window->width());
            const int h = std::max(1, window->height());
            blurHelper.applyToWindow(window, true, QRegion(0, 0, w, h));
        };
        QObject::connect(window, &QWindow::widthChanged, window, [updateWindowBlur](int) { updateWindowBlur(); });
        QObject::connect(window, &QWindow::heightChanged, window, [updateWindowBlur](int) { updateWindowBlur(); });
        window->show();
        updateWindowBlur();
    }

    return app.exec();
}
