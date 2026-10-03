#include "SettingsBridge.hpp"
#include "settings/WifiManager.hpp"
#include <common/logger.hpp>
#include <common/version.hpp>
#include <common/DBusNames.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFileInfo>
#include <QUrl>
#include <QImage>
#include <iostream>
#include <unistd.h>

int main(int argc, char* argv[]) {
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        qputenv("QSG_RHI_BACKEND", "software");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-wifi-render"));
    app.setDesktopFileName(tinexus::common::dbus::qapp_id::Settings());

    std::cout << "[Visual Test (Qt6)] Initializing Wi-Fi Render Test..." << std::endl;

    tinexus::settings_ui::SettingsBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    QString qmlPath;
    if (qEnvironmentVariableIsSet("TINEXUS_SETTINGS_QML")) {
        qmlPath = qEnvironmentVariable("TINEXUS_SETTINGS_QML");
    } else {
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList candidates = {
            appDir + QStringLiteral("/../src/settings-ui/qml/MainWindow.qml"),
            appDir + QStringLiteral("/qml/MainWindow.qml"),
            QStringLiteral("src/settings-ui/qml/MainWindow.qml"),
            QStringLiteral("/mnt/e/Tinu's Technology/Tinexus Manager/src/settings-ui/qml/MainWindow.qml")
        };
        for (const auto& cand : candidates) {
            if (QFileInfo::exists(cand)) {
                qmlPath = cand;
                break;
            }
        }
    }

    if (qmlPath.isEmpty() || !QFileInfo::exists(qmlPath)) {
        std::cerr << "FAIL: Could not locate MainWindow.qml" << std::endl;
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: Failed to load root QML object" << std::endl;
        return 1;
    }

    auto window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) {
        std::cerr << "FAIL: Root object is not a QQuickWindow" << std::endl;
        return 1;
    }

    window->show();

    auto grab_and_save = [&](const std::string& filename) -> bool {
        for (int i = 0; i < 8; ++i) {
            app.processEvents();
            usleep(25000);
        }
        QImage img = window->grabWindow();
        if (!img.isNull()) {
            img.save(QString::fromStdString(filename));
            std::cout << "[Visual Test (Qt6)] Successfully saved " << filename
                      << " (" << img.width() << "x" << img.height() << ")" << std::endl;
            return true;
        } else {
            std::cerr << "FAIL: grabWindow returned null for " << filename << std::endl;
            return false;
        }
    };

    // 1. Render Network / Wi-Fi Page
    bridge.selectPage(3);
    bridge.triggerWifiScan();
    if (!grab_and_save("wifi_settings_network_page.png")) return 1;

    // 2. Render Wi-Fi Password Modal Dialog
    bridge.openWifiModal(QStringLiteral("Tinexus-TestNet-5G"));
    if (!grab_and_save("wifi_settings_modal_dialog.png")) return 1;
    bridge.closeWifiModal();

    std::cout << "=== Wi-Fi UI visual tests completed successfully! ===" << std::endl;
    return 0;
}
