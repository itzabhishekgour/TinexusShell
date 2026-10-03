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
    // Force offscreen and software RHI for reliable headless rendering in container/CI
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    if (!qEnvironmentVariableIsSet("QSG_RHI_BACKEND")) {
        qputenv("QSG_RHI_BACKEND", "software");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-settings-render"));
    app.setDesktopFileName(tinexus::common::dbus::qapp_id::Settings());

    std::cout << "=== Tinexus Settings UI Visual Test Suite (Qt6) ===" << std::endl;

    tinexus::settings_ui::SettingsBridge bridge;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    // Locate QML
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

    auto save_page = [&](const std::string& name1, const std::string& name2) -> bool {
        for (int i = 0; i < 8; ++i) {
            app.processEvents();
            usleep(25000); // 25ms settling per step
        }
        QImage img = window->grabWindow();
        if (!img.isNull()) {
            img.save(QString::fromStdString(name1));
            img.save(QString::fromStdString(name2));
            std::cout << "[Visual Test (Qt6)] Successfully saved " << name1 << " and " << name2
                      << " (" << img.width() << "x" << img.height() << ")" << std::endl;
            return true;
        } else {
            std::cerr << "FAIL: grabWindow returned null for " << name1 << std::endl;
            return false;
        }
    };

    // 1. Display Page
    bridge.selectPage(0);
    if (!save_page("settings_display_page.png", "setting_display_page.png")) return 1;

    // 1b. Sound Page
    bridge.selectPage(1);
    if (!save_page("settings_sound_page.png", "setting_sound_page.png")) return 1;

    // 2. Personalization Page
    bridge.selectPage(2);
    if (!save_page("settings_personalization_page.png", "setting_personalization_page.png")) return 1;

    // 3. Network / Wi-Fi Page
    bridge.selectPage(3);
    bridge.triggerWifiScan();
    if (!save_page("settings_network_wifi_page.png", "setting_network_wifi_page.png")) return 1;

    // 4. Wi-Fi Password Modal Dialog
    bridge.openWifiModal(QStringLiteral("Tinexus-5G-Ultra"));
    if (!save_page("settings_wifi_modal_open.png", "setting_wifi_modal_open.png")) return 1;
    bridge.closeWifiModal();

    // 5. System & Power Page
    bridge.selectPage(4);
    if (!save_page("settings_system_page.png", "setting_system_page.png")) return 1;

    // 6. Keyboard Shortcuts Page
    bridge.selectPage(5);
    if (!save_page("settings_shortcuts_page.png", "setting_shortcuts_page.png")) return 1;

    // 7. Privacy & Security Page
    bridge.selectPage(6);
    if (!save_page("settings_privacy_page.png", "setting_privacy_page.png")) return 1;

    // 8. About Page
    bridge.selectPage(7);
    if (!save_page("settings_about_page.png", "setting_about_page.png")) return 1;

    std::cout << "=== All 8 Settings UI visual tests completed successfully! ===" << std::endl;
    return 0;
}
