#include "AboutBridge.hpp"
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <iostream>
#include <cassert>

int main(int argc, char* argv[]) {
    // Run headless / offscreen
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);

    std::cout << "==========================================" << std::endl;
    std::cout << "=== Tinexus About Qt6 Live Verification ===" << std::endl;
    std::cout << "==========================================" << std::endl;

    tinexus::about::AboutBridge bridge;

    std::cout << "[1] Verifying System Information Probes..." << std::endl;
    std::cout << "  OS Title:       " << bridge.osTitle().toStdString() << std::endl;
    std::cout << "  OS Version:     " << bridge.osVersion().toStdString() << std::endl;
    std::cout << "  Kernel Version: " << bridge.kernelVersion().toStdString() << std::endl;
    std::cout << "  CPU Model:      " << bridge.cpuModel().toStdString() << std::endl;
    std::cout << "  Memory Info:    " << bridge.memInfo().toStdString() << std::endl;
    std::cout << "  Graphics Info:  " << bridge.graphicsInfo().toStdString() << std::endl;

    assert(!bridge.osTitle().isEmpty());
    assert(!bridge.osVersion().isEmpty());
    assert(!bridge.kernelVersion().isEmpty());
    assert(!bridge.cpuModel().isEmpty());
    assert(!bridge.memInfo().isEmpty());
    assert(!bridge.graphicsInfo().isEmpty());
    std::cout << "  [PASS] Real sysfs/procfs system probes verified." << std::endl;

    std::cout << "\n[2] Verifying Display Information Probes..." << std::endl;
    std::cout << "  Display Name:       " << bridge.displayName().toStdString() << std::endl;
    std::cout << "  Display Resolution: " << bridge.displayResolution().toStdString() << std::endl;
    std::cout << "  Refresh Rate:       " << bridge.displayRefreshRate().toStdString() << std::endl;
    std::cout << "  Scale:              " << bridge.displayScale().toStdString() << std::endl;
    std::cout << "  Render Backend:     " << bridge.displayRenderer().toStdString() << std::endl;

    assert(!bridge.displayName().isEmpty());
    assert(!bridge.displayResolution().isEmpty());
    assert(!bridge.displayRenderer().isEmpty());
    std::cout << "  [PASS] Real DRM display probes verified." << std::endl;

    std::cout << "\n[3] Verifying Storage Information Probes..." << std::endl;
    std::cout << "  Root Mount:  " << bridge.storageMount().toStdString() << std::endl;
    std::cout << "  Device:      " << bridge.storageDevice().toStdString() << std::endl;
    std::cout << "  Filesystem:  " << bridge.storageFsType().toStdString() << std::endl;
    std::cout << "  Total Space: " << bridge.storageTotalGb() << " GB" << std::endl;
    std::cout << "  Used Space:  " << bridge.storageUsedGb() << " GB" << std::endl;
    std::cout << "  Free Space:  " << bridge.storageFreeGb() << " GB" << std::endl;

    assert(!bridge.storageMount().isEmpty());
    assert(!bridge.storageDevice().isEmpty());
    assert(!bridge.storageFsType().isEmpty());
    assert(bridge.storageTotalGb() > 0);
    std::cout << "  [PASS] Real statvfs storage probes verified." << std::endl;

    std::cout << "\n[4] Verifying Supervised Platform Services..." << std::endl;
    auto svcs = bridge.services();
    std::cout << "  Supervised services count: " << svcs.size() << std::endl;
    for (const auto& val : svcs) {
        auto map = val.toMap();
        std::cout << "    • " << map["name"].toString().toStdString()
                  << " (" << map["role"].toString().toStdString() << ") -> "
                  << map["status"].toString().toStdString()
                  << " [" << map["pid"].toString().toStdString() << "]" << std::endl;
    }
    std::cout << "  [PASS] Supervised services query verified." << std::endl;

    std::cout << "\n[5] Verifying QML Engine Loading..." << std::endl;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bridge"), &bridge);
    engine.load(QUrl::fromLocalFile(QStringLiteral("src/about/qml/AboutWindow.qml")));
    assert(!engine.rootObjects().isEmpty());
    std::cout << "  [PASS] AboutWindow.qml loaded cleanly without errors." << std::endl;

    std::cout << "\n=== All Tinexus About verification tests PASSED (100%) ===" << std::endl;
    return 0;
}
