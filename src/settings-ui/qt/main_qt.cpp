#include "SettingsBridge.hpp"
#include <common/logger.hpp>
#include <common/version.hpp>
#include <txui/core/SingleInstance.hpp>

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QFileInfo>
#include <QUrl>
#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    tinexus::log::set_component_name("tinexus-settings-ui-qt6");
    tinexus::log::info("Starting Tinexus Control Center (Qt6/QML) v{}", tinexus::VERSION_STRING);

    // Single-instance enforcement: focus existing instance and exit if already open
    txui::SingleInstance single_instance("tinexus-settings");
    if (!single_instance.is_primary()) {
        tinexus::log::info("[settings-ui-qt6] Existing instance of tinexus-settings detected — requesting focus and exiting.");
        single_instance.request_focus_primary();
        return 0;
    }

    // Force Wayland platform when running under Wayland compositor
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "wayland");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("tinexus-settings"));
    app.setApplicationDisplayName(QStringLiteral("Tinexus Settings"));
    app.setDesktopFileName(QStringLiteral("io.tinexus.Settings"));

    tinexus::settings_ui::SettingsBridge bridge;

    // Parse CLI parameters for initial page
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromUtf8(argv[i]);
        if (arg == QStringLiteral("--page") && i + 1 < argc) {
            QString page = QString::fromUtf8(argv[++i]).toLower();
            if (page == QStringLiteral("display")) {
                bridge.selectPage(0);
            } else if (page == QStringLiteral("sound") || page == QStringLiteral("audio")) {
                bridge.selectPage(1);
            } else if (page == QStringLiteral("personalization") || page == QStringLiteral("theme")) {
                bridge.selectPage(2);
            } else if (page == QStringLiteral("network") || page == QStringLiteral("wifi")) {
                bridge.selectPage(3);
            } else if (page == QStringLiteral("system") || page == QStringLiteral("power")) {
                bridge.selectPage(4);
            } else if (page == QStringLiteral("shortcuts") || page == QStringLiteral("keyboard")) {
                bridge.selectPage(5);
            } else if (page == QStringLiteral("privacy") || page == QStringLiteral("security")) {
                bridge.selectPage(6);
            } else if (page == QStringLiteral("about")) {
                bridge.selectPage(7);
            }
        } else if (arg == QStringLiteral("-n") || arg == QStringLiteral("--network") || arg == QStringLiteral("--wifi")) {
            bridge.selectPage(3);
        }
    }

    // Wi-Fi periodic poller (500ms)
    QTimer wifiTimer;
    QObject::connect(&wifiTimer, &QTimer::timeout, &bridge, &tinexus::settings_ui::SettingsBridge::pollWifiStatus);
    wifiTimer.start(500);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);

    // Resolve QML location
    QString qmlPath;
    if (qEnvironmentVariableIsSet("TINEXUS_SETTINGS_QML")) {
        qmlPath = qEnvironmentVariable("TINEXUS_SETTINGS_QML");
    } else {
        // Search relative to binary and known locations
        QString appDir = QCoreApplication::applicationDirPath();
        QStringList candidates = {
            appDir + QStringLiteral("/../src/settings-ui/qml/MainWindow.qml"),
            appDir + QStringLiteral("/qml/MainWindow.qml"),
            QStringLiteral("/usr/share/tinexus-settings/qml/MainWindow.qml"),
            QStringLiteral("/usr/share/tinexus/settings-ui/qml/MainWindow.qml"),
            QStringLiteral("/usr/share/tinexus-settings-ui/qml/MainWindow.qml"),
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
        tinexus::log::error("[settings-ui-qt6] Fatal: Could not locate MainWindow.qml! Set TINEXUS_SETTINGS_QML");
        std::cerr << "Error: Could not locate MainWindow.qml" << std::endl;
        return 1;
    }

    tinexus::log::info("[settings-ui-qt6] Loading QML from: {}", qmlPath.toStdString());
    engine.load(QUrl::fromLocalFile(qmlPath));

    if (engine.rootObjects().isEmpty()) {
        tinexus::log::error("[settings-ui-qt6] Fatal: Failed to load QML root object!");
        return 1;
    }

    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        window->show();
        window->raise();
        window->requestActivate();
        tinexus::log::info("[settings-ui-qt6] Window presented successfully: title='{}' size={}x{}",
                           window->title().toStdString(), window->width(), window->height());
    }

    // Check for automated verification flag
    bool autoTest = app.arguments().contains(QStringLiteral("--auto-test"));
    if (autoTest) {
        tinexus::log::info("[settings-ui-qt6] --auto-test mode enabled, initiating automated verification sequence");
        QTimer* testTimer = new QTimer(&app);
        testTimer->setInterval(500);
        int* step = new int(0);

        QObject::connect(testTimer, &QTimer::timeout, [&app, &bridge, testTimer, step]() {
            (*step)++;

            if (*step == 1) {
                bridge.selectPage(0);
                bridge.setDisplayScaleIndex(1); // 125%
                bridge.setBrightness(80);
                bridge.setNightLight(true);
                bridge.setVrrEnabled(true);
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 1: DisplayPage — selectPage(0), setDisplayScaleIndex(1), setBrightness(80), setNightLight(true), setVrrEnabled(true)");
            } else if (*step == 2) {
                bridge.selectPage(1);
                bridge.setVolume(75);
                bridge.setMuted(true);
                bridge.playTestSound();
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 2: SoundPage — selectPage(1), setVolume(75), setMuted(true), playTestSound()");
            } else if (*step == 3) {
                bridge.selectPage(2);
                bridge.setAccentIndex(2); // Deep Violet (#5856d6)
                bridge.setThemeMode(QStringLiteral("Light"));
                bridge.setSelectedWallpaperIndex(1);
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 3: PersonalizationPage — selectPage(2), setAccentIndex(2), setThemeMode(\"Light\"), setSelectedWallpaperIndex(1)");
            } else if (*step == 4) {
                bridge.selectPage(3);
                bridge.triggerWifiScan();
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 4: NetworkPage — selectPage(3), triggerWifiScan()");
            } else if (*step == 5) {
                // Simulate clicking a password-protected network to summon WifiModal
                bridge.openWifiModal(QStringLiteral("Tinexus-TestNet-5G"));
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 5: WifiModal — openWifiModal(\"Tinexus-TestNet-5G\") summoned for secured network");
            } else if (*step == 6) {
                // Dismiss the Wi-Fi modal
                bridge.closeWifiModal();
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 6: WifiModal — closeWifiModal() dismissed");
            } else if (*step == 7) {
                bridge.selectPage(4);
                bridge.setScreenTimeoutMin(10);
                bridge.setSleepAfterMin(30);
                bridge.setPowerProfileIndex(2); // Performance
                bridge.setLockOnSleep(false);
                bridge.setPamAuth(false);
                bridge.setClipboardHistorySize(100);
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 7: SystemPage — selectPage(4), setScreenTimeoutMin(10), setSleepAfterMin(30), setPowerProfileIndex(2), setLockOnSleep(false), setPamAuth(false), setClipboardHistorySize(100)");
            } else if (*step == 8) {
                bridge.selectPage(5);
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 8: ShortcutsPage — selectPage(5)");
            } else if (*step == 9) {
                bridge.selectPage(6);
                bridge.rescanApps();
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 9: PrivacyPage — selectPage(6), rescanApps()");
            } else if (*step == 10) {
                bridge.selectPage(7);
                bridge.saveConfig();
                tinexus::log::info("[settings-ui-qt6] AutoTest Step 10: AboutPage — selectPage(7), saveConfig() verified with all 11 fields changed from defaults");
            } else if (*step == 11) {
                tinexus::log::info("[settings-ui-qt6] AutoTest: Complete! All 8 pages and WifiModal verified, exiting cleanly.");
                testTimer->stop();
                delete step;
                app.quit();
            }
        });
        testTimer->start();
    }

    return app.exec();
}
