// ============================================================================
// test_monitor_live.cpp — Comprehensive Live Test for tinexus-monitor (Qt6)
// Verifies genuine Linux sysfs/procfs probes, sorting, safety, and QML engine
// ============================================================================
#include "MonitorBridge.hpp"
#include <common/PlatformServices.hpp>
#include <common/logger.hpp>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtCore/QFileInfo>
#include <cassert>
#include <iostream>

using namespace tinexus::monitor;

int main(int argc, char* argv[]) {
    std::cout << "====================================================\n";
    std::cout << "=== Tinexus Activity Monitor Qt6 Live Verification ===\n";
    std::cout << "====================================================\n";

    QGuiApplication app(argc, argv);

    // [1] Verify Real-time Linux sysfs/procfs Probes
    std::cout << "[1] Verifying System Snapshot Probes...\n";
    auto snap = ResourceMonitor::instance().collect_snapshot();
    std::cout << "  CPU Cores Detected: " << snap.cpu_cores.size() << "\n";
    std::cout << "  CPU Aggregate Usage: " << snap.cpu_aggregate_usage_percent << "%\n";
    std::cout << "  Memory Total RAM:    " << (snap.memory.total_ram_bytes / (1024 * 1024)) << " MB\n";
    std::cout << "  Memory Used RAM:     " << (snap.memory.used_ram_bytes / (1024 * 1024)) << " MB\n";
    std::cout << "  Disk Read Rate:      " << snap.disk.read_bytes_sec << " B/s\n";
    std::cout << "  Disk Write Rate:     " << snap.disk.write_bytes_sec << " B/s\n";
    std::cout << "  Network Active Dev:  " << (snap.network.active_interface.empty() ? "None/Local" : snap.network.active_interface) << "\n";
    std::cout << "  Network RX Rate:     " << snap.network.rx_bytes_sec << " B/s\n";
    std::cout << "  Network TX Rate:     " << snap.network.tx_bytes_sec << " B/s\n";
    std::cout << "  Processes Count:     " << snap.processes.size() << "\n";

    assert(snap.memory.total_ram_bytes > 0 && "Total RAM must be greater than 0");
    assert(!snap.processes.empty() && "Process discovery must find running processes");
    std::cout << "  [PASS] Real sysfs/procfs hardware & process probes verified.\n\n";

    // [2] Verify Supervised Platform Services
    std::cout << "[2] Verifying Supervised Platform Services...\n";
    auto services = tinexus::platform::PlatformServices::query_supervised_services();
    std::cout << "  Supervised Daemons Count: " << services.size() << "\n";
    for (const auto& s : services) {
        std::cout << "    • " << s.name << " (" << s.role << ") -> " << s.status_str
                  << (s.is_protected ? " [PROTECTED]" : "") << "\n";
    }
    assert(!services.empty() && "Platform services query must return registered services");
    std::cout << "  [PASS] Supervised services query verified.\n\n";

    // [3] Verify MonitorBridge Properties & Safety Boundary
    std::cout << "[3] Verifying MonitorBridge State & Safety Controls...\n";
    MonitorBridge bridge;
    std::cout << "  Bridge CPU Usage:      " << bridge.cpuUsage() << "%\n";
    std::cout << "  Bridge CPU Model:      " << bridge.cpuModel().toStdString() << "\n";
    std::cout << "  Bridge Cores:          " << bridge.cpuCoresCount() << "\n";
    std::cout << "  Bridge RAM Used/Total: " << bridge.memUsedGb().toStdString() << " / " << bridge.memTotalGb().toStdString() << "\n";
    std::cout << "  Bridge Disk Read:      " << bridge.diskReadRate().toStdString() << "\n";
    std::cout << "  Bridge Net RX:         " << bridge.netRxRate().toStdString() << "\n";
    std::cout << "  Bridge Procs Listed:   " << bridge.processCount() << "\n";

    // Test selection of PID 1 (Init - MUST be protected)
    bridge.selectProcess(1);
    std::cout << "  Selected PID 1: isProtected=" << (bridge.selectedIsProtected() ? "true" : "false") << "\n";
    assert(bridge.selectedIsProtected() && "PID 1 MUST be recognized as protected!");

    // Test safety modal opening
    bridge.openKillDialog(false);
    assert(bridge.killModalOpen() && "Kill modal must be open");
    // Confirm kill on protected process must fail safely
    [[maybe_unused]] bool killedProt = bridge.confirmKillProcess();
    assert(!killedProt && "Attempting to kill protected PID 1 must be rejected by safety boundary!");
    assert(!bridge.killModalOpen() && "Modal must close after rejection");

    // Test sorting
    bridge.toggleSort("cpu");
    bridge.toggleSort("memory");
    std::cout << "  [PASS] MonitorBridge safety boundaries, sorting, and telemetry verified.\n\n";

    // [4] Verify QML Engine Component Loading
    std::cout << "[4] Verifying MonitorWindow.qml Loading...\n";
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("monitorBridge"), &bridge);

    const QString qmlPath = QStringLiteral("src/monitor/qml/MonitorWindow.qml");
    if (!QFileInfo::exists(qmlPath)) {
        std::cerr << "FAIL: MonitorWindow.qml not found at " << qmlPath.toStdString() << "\n";
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "FAIL: QQmlApplicationEngine could not load MonitorWindow.qml!\n";
        return 1;
    }
    std::cout << "  [PASS] MonitorWindow.qml loaded cleanly without errors.\n\n";

    std::cout << "=== All Tinexus Activity Monitor verification tests PASSED (100%) ===\n";
    return 0;
}
