#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QTimer>
#include <atomic>
#include <memory>
#include "monitor/resource_monitor.hpp"
#include "monitor/process_controller.hpp"
#include "common/PlatformServices.hpp"

namespace tinexus::monitor {

class MonitorBridge : public QObject {
    Q_OBJECT

    // CPU & Thermal
    Q_PROPERTY(qreal cpuUsage READ cpuUsage NOTIFY telemetryChanged)
    Q_PROPERTY(qreal cpuTemperature READ cpuTemperature NOTIFY telemetryChanged)
    Q_PROPERTY(QString cpuTemperatureStr READ cpuTemperatureStr NOTIFY telemetryChanged)
    Q_PROPERTY(int cpuCoresCount READ cpuCoresCount NOTIFY telemetryChanged)
    Q_PROPERTY(QString cpuModel READ cpuModel NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList coreUsages READ coreUsages NOTIFY telemetryChanged)

    // Memory (RAM + Cached + Swap)
    Q_PROPERTY(qulonglong memUsedBytes READ memUsedBytes NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong memTotalBytes READ memTotalBytes NOTIFY telemetryChanged)
    Q_PROPERTY(QString memUsedGb READ memUsedGb NOTIFY telemetryChanged)
    Q_PROPERTY(QString memTotalGb READ memTotalGb NOTIFY telemetryChanged)
    Q_PROPERTY(QString memCachedGb READ memCachedGb NOTIFY telemetryChanged)
    Q_PROPERTY(QString memSwapUsedGb READ memSwapUsedGb NOTIFY telemetryChanged)
    Q_PROPERTY(QString memSwapTotalGb READ memSwapTotalGb NOTIFY telemetryChanged)
    Q_PROPERTY(qreal memUsagePct READ memUsagePct NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList memHistory READ memHistory NOTIFY historyChanged)

    // Disk I/O
    Q_PROPERTY(QString diskReadRate READ diskReadRate NOTIFY telemetryChanged)
    Q_PROPERTY(QString diskWriteRate READ diskWriteRate NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong diskReadBytesSec READ diskReadBytesSec NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong diskWriteBytesSec READ diskWriteBytesSec NOTIFY telemetryChanged)
    Q_PROPERTY(uint diskIops READ diskIops NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList diskHistory READ diskHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList diskReadHistory READ diskReadHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList diskWriteHistory READ diskWriteHistory NOTIFY historyChanged)

    // Network I/O
    Q_PROPERTY(QString netRxRate READ netRxRate NOTIFY telemetryChanged)
    Q_PROPERTY(QString netTxRate READ netTxRate NOTIFY telemetryChanged)
    Q_PROPERTY(QString netActiveInterface READ netActiveInterface NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong netRxBytesSec READ netRxBytesSec NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong netTxBytesSec READ netTxBytesSec NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList netHistory READ netHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList netRxHistory READ netRxHistory NOTIFY historyChanged)
    Q_PROPERTY(QVariantList netTxHistory READ netTxHistory NOTIFY historyChanged)

    // GPU Telemetry
    Q_PROPERTY(QString gpuVendor READ gpuVendor NOTIFY telemetryChanged)
    Q_PROPERTY(QString gpuDeviceName READ gpuDeviceName NOTIFY telemetryChanged)
    Q_PROPERTY(QString gpuDriver READ gpuDriver NOTIFY telemetryChanged)
    Q_PROPERTY(QString gpuTelemetryStatus READ gpuTelemetryStatus NOTIFY telemetryChanged)
    Q_PROPERTY(qreal gpuBusyPct READ gpuBusyPct NOTIFY telemetryChanged)
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList gpuHistory READ gpuHistory NOTIFY historyChanged)

    // Power / Battery
    Q_PROPERTY(bool hasBattery READ hasBattery NOTIFY telemetryChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY telemetryChanged)
    Q_PROPERTY(QString powerStatus READ powerStatus NOTIFY telemetryChanged)
    Q_PROPERTY(qreal powerWatts READ powerWatts NOTIFY telemetryChanged)

    // Processes & Selection
    Q_PROPERTY(int processCount READ processCount NOTIFY processesChanged)
    Q_PROPERTY(QVariantList processes READ processes NOTIFY processesChanged)

    Q_PROPERTY(QString searchQuery READ searchQuery WRITE setSearchQuery NOTIFY filterChanged)
    Q_PROPERTY(int category READ category WRITE setCategory NOTIFY filterChanged)
    Q_PROPERTY(QString sortColumn READ sortColumn WRITE setSortColumn NOTIFY filterChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending WRITE setSortAscending NOTIFY filterChanged)

    Q_PROPERTY(int selectedPid READ selectedPid NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedProcessName READ selectedProcessName NOTIFY selectionChanged)
    Q_PROPERTY(bool selectedIsProtected READ selectedIsProtected NOTIFY selectionChanged)

    Q_PROPERTY(bool killModalOpen READ killModalOpen NOTIFY killModalChanged)
    Q_PROPERTY(bool killModalForce READ killModalForce NOTIFY killModalChanged)

    Q_PROPERTY(bool inspectModalOpen READ inspectModalOpen NOTIFY inspectModalChanged)
    Q_PROPERTY(QVariantMap inspectedProcess READ inspectedProcess NOTIFY inspectModalChanged)

    Q_PROPERTY(QVariantList services READ services NOTIFY servicesChanged)

    Q_PROPERTY(int activeTab READ activeTab WRITE setActiveTab NOTIFY activeTabChanged)
    Q_PROPERTY(bool isVisible READ isVisible WRITE setIsVisible NOTIFY isVisibleChanged)

public:
    explicit MonitorBridge(QObject* parent = nullptr);
    ~MonitorBridge() override = default;

    int activeTab() const { return m_activeTab; }
    void setActiveTab(int tab);

    bool isVisible() const { return m_isVisible; }
    void setIsVisible(bool v);

    qreal cpuUsage() const { return m_cpuUsage; }
    qreal cpuTemperature() const { return m_cpuTemperature; }
    QString cpuTemperatureStr() const { return m_cpuTemperatureStr; }
    int cpuCoresCount() const { return m_cpuCoresCount; }
    QString cpuModel() const { return m_cpuModel; }
    QVariantList cpuHistory() const { return m_cpuHistory; }
    QVariantList coreUsages() const { return m_coreUsages; }

    qulonglong memUsedBytes() const { return m_memUsedBytes; }
    qulonglong memTotalBytes() const { return m_memTotalBytes; }
    QString memUsedGb() const { return m_memUsedGb; }
    QString memTotalGb() const { return m_memTotalGb; }
    QString memCachedGb() const { return m_memCachedGb; }
    QString memSwapUsedGb() const { return m_memSwapUsedGb; }
    QString memSwapTotalGb() const { return m_memSwapTotalGb; }
    qreal memUsagePct() const { return m_memUsagePct; }
    QVariantList memHistory() const { return m_memHistory; }

    QString diskReadRate() const { return m_diskReadRate; }
    QString diskWriteRate() const { return m_diskWriteRate; }
    qulonglong diskReadBytesSec() const { return m_diskReadBytesSec; }
    qulonglong diskWriteBytesSec() const { return m_diskWriteBytesSec; }
    uint diskIops() const { return m_diskIops; }
    QVariantList diskHistory() const { return m_diskHistory; }
    QVariantList diskReadHistory() const { return m_diskReadHistory; }
    QVariantList diskWriteHistory() const { return m_diskWriteHistory; }

    QString netRxRate() const { return m_netRxRate; }
    QString netTxRate() const { return m_netTxRate; }
    QString netActiveInterface() const { return m_netActiveInterface; }
    qulonglong netRxBytesSec() const { return m_netRxBytesSec; }
    qulonglong netTxBytesSec() const { return m_netTxBytesSec; }
    QVariantList netHistory() const { return m_netHistory; }
    QVariantList netRxHistory() const { return m_netRxHistory; }
    QVariantList netTxHistory() const { return m_netTxHistory; }

    QString gpuVendor() const { return m_gpuVendor; }
    QString gpuDeviceName() const { return m_gpuDeviceName; }
    QString gpuDriver() const { return m_gpuDriver; }
    QString gpuTelemetryStatus() const { return m_gpuTelemetryStatus; }
    qreal gpuBusyPct() const { return m_gpuBusyPct; }
    bool gpuAvailable() const { return m_gpuBusyPct >= 0.0; }
    QVariantList gpuHistory() const { return m_gpuHistory; }

    bool hasBattery() const { return m_hasBattery; }
    int batteryPercent() const { return m_batteryPercent; }
    QString powerStatus() const { return m_powerStatus; }
    qreal powerWatts() const { return m_powerWatts; }

    int processCount() const { return static_cast<int>(m_cachedProcesses.size()); }
    QVariantList processes() const { return m_displayProcesses; }

    QString searchQuery() const { return m_searchQuery; }
    void setSearchQuery(const QString& q);

    int category() const { return m_category; }
    void setCategory(int cat);

    QString sortColumn() const { return m_sortColumn; }
    void setSortColumn(const QString& col);

    bool sortAscending() const { return m_sortAscending; }
    void setSortAscending(bool asc);

    int selectedPid() const { return m_selectedPid; }
    QString selectedProcessName() const { return m_selectedProcessName; }
    bool selectedIsProtected() const { return m_selectedIsProtected; }

    bool killModalOpen() const { return m_killModalOpen; }
    bool killModalForce() const { return m_killModalForce; }

    bool inspectModalOpen() const { return m_inspectModalOpen; }
    QVariantMap inspectedProcess() const { return m_inspectedProcess; }

    QVariantList services() const { return m_services; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectProcess(int pid);
    Q_INVOKABLE void toggleSort(const QString& col);
    Q_INVOKABLE void openKillDialog(bool force);
    Q_INVOKABLE void closeKillDialog();
    Q_INVOKABLE bool confirmKillProcess();
    Q_INVOKABLE QVariantMap inspectProcess(int pid);
    Q_INVOKABLE void closeInspectModal();
    Q_INVOKABLE bool restartService(const QString& name);

signals:
    void telemetryChanged();
    void historyChanged();
    void processesChanged();
    void filterChanged();
    void selectionChanged();
    void killModalChanged();
    void inspectModalChanged();
    void servicesChanged();
    void activeTabChanged();
    void isVisibleChanged();

private:
    void refreshSync(bool collectProcs, bool collectServices);
    void applySnapshot(const SystemSnapshot& snap, bool hasProcs);
    void applyServicesList(const std::vector<tinexus::platform::ServiceInfo>& svcs);
    void updateProcessesList();
    static QString formatBytes(uint64_t bytes);
    static QString formatRate(uint64_t bytesSec);
    static void appendRing(QVariantList& ring, qreal value, int maxLen = 60);

    qreal m_cpuUsage{0.0};
    qreal m_cpuTemperature{-1.0};
    QString m_cpuTemperatureStr{QStringLiteral("N/A")};
    int m_cpuCoresCount{1};
    QString m_cpuModel;
    QVariantList m_cpuHistory;
    QVariantList m_coreUsages;

    qulonglong m_memUsedBytes{0};
    qulonglong m_memTotalBytes{0};
    QString m_memUsedGb{"0.0 GB"};
    QString m_memTotalGb{"0.0 GB"};
    QString m_memCachedGb{"0.0 GB"};
    QString m_memSwapUsedGb{"0.0 GB"};
    QString m_memSwapTotalGb{"0.0 GB"};
    qreal m_memUsagePct{0.0};
    QVariantList m_memHistory;

    QString m_diskReadRate{"0 KB/s"};
    QString m_diskWriteRate{"0 KB/s"};
    qulonglong m_diskReadBytesSec{0};
    qulonglong m_diskWriteBytesSec{0};
    uint m_diskIops{0};
    QVariantList m_diskHistory;
    QVariantList m_diskReadHistory;
    QVariantList m_diskWriteHistory;

    QString m_netRxRate{"0 KB/s"};
    QString m_netTxRate{"0 KB/s"};
    QString m_netActiveInterface{"None"};
    qulonglong m_netRxBytesSec{0};
    qulonglong m_netTxBytesSec{0};
    QVariantList m_netHistory;
    QVariantList m_netRxHistory;
    QVariantList m_netTxHistory;

    QString m_gpuVendor{"Integrated DRM"};
    QString m_gpuDeviceName{"DRM Scanout Graphics"};
    QString m_gpuDriver{"modesetting"};
    QString m_gpuTelemetryStatus{"Direct Scanout"};
    qreal m_gpuBusyPct{-1.0};
    QVariantList m_gpuHistory;

    bool m_hasBattery{false};
    int m_batteryPercent{100};
    QString m_powerStatus{"AC Powered"};
    qreal m_powerWatts{0.0};

    std::vector<ProcessInfo> m_cachedProcesses;
    QVariantList m_displayProcesses;

    QString m_searchQuery;
    int m_category{0}; // 0: All, 1: My Processes, 2: System
    QString m_sortColumn{"cpu"};
    bool m_sortAscending{false};

    int m_selectedPid{-1};
    QString m_selectedProcessName;
    bool m_selectedIsProtected{false};

    bool m_killModalOpen{false};
    bool m_killModalForce{false};

    bool m_inspectModalOpen{false};
    QVariantMap m_inspectedProcess;

    QVariantList m_services;

    int m_activeTab{0};
    bool m_isVisible{true};

    uid_t m_currentUid{0};
    std::vector<tinexus::platform::ServiceInfo> m_cachedServices;
    std::shared_ptr<std::atomic<bool>> m_refreshing{std::make_shared<std::atomic<bool>>(false)};
};

} // namespace tinexus::monitor
