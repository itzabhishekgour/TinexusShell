#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtCore/QTimer>
#include "monitor/resource_monitor.hpp"
#include "monitor/process_controller.hpp"
#include "common/PlatformServices.hpp"

namespace tinexus::monitor {

class MonitorBridge : public QObject {
    Q_OBJECT

    // Telemetry properties
    Q_PROPERTY(qreal cpuUsage READ cpuUsage NOTIFY telemetryChanged)
    Q_PROPERTY(qreal cpuTemperature READ cpuTemperature NOTIFY telemetryChanged)
    Q_PROPERTY(int cpuCoresCount READ cpuCoresCount NOTIFY telemetryChanged)
    Q_PROPERTY(QString cpuModel READ cpuModel NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY historyChanged)

    Q_PROPERTY(qulonglong memUsedBytes READ memUsedBytes NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong memTotalBytes READ memTotalBytes NOTIFY telemetryChanged)
    Q_PROPERTY(QString memUsedGb READ memUsedGb NOTIFY telemetryChanged)
    Q_PROPERTY(QString memTotalGb READ memTotalGb NOTIFY telemetryChanged)
    Q_PROPERTY(qreal memUsagePct READ memUsagePct NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList memHistory READ memHistory NOTIFY historyChanged)

    Q_PROPERTY(QString diskReadRate READ diskReadRate NOTIFY telemetryChanged)
    Q_PROPERTY(QString diskWriteRate READ diskWriteRate NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong diskReadBytesSec READ diskReadBytesSec NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong diskWriteBytesSec READ diskWriteBytesSec NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList diskHistory READ diskHistory NOTIFY historyChanged)

    Q_PROPERTY(QString netRxRate READ netRxRate NOTIFY telemetryChanged)
    Q_PROPERTY(QString netTxRate READ netTxRate NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong netRxBytesSec READ netRxBytesSec NOTIFY telemetryChanged)
    Q_PROPERTY(qulonglong netTxBytesSec READ netTxBytesSec NOTIFY telemetryChanged)
    Q_PROPERTY(QVariantList netHistory READ netHistory NOTIFY historyChanged)

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

    Q_PROPERTY(QVariantList services READ services NOTIFY servicesChanged)

public:
    explicit MonitorBridge(QObject* parent = nullptr);
    ~MonitorBridge() override = default;

    qreal cpuUsage() const { return m_cpuUsage; }
    qreal cpuTemperature() const { return m_cpuTemperature; }
    int cpuCoresCount() const { return m_cpuCoresCount; }
    QString cpuModel() const { return m_cpuModel; }
    QVariantList cpuHistory() const { return m_cpuHistory; }

    qulonglong memUsedBytes() const { return m_memUsedBytes; }
    qulonglong memTotalBytes() const { return m_memTotalBytes; }
    QString memUsedGb() const { return m_memUsedGb; }
    QString memTotalGb() const { return m_memTotalGb; }
    qreal memUsagePct() const { return m_memUsagePct; }
    QVariantList memHistory() const { return m_memHistory; }

    QString diskReadRate() const { return m_diskReadRate; }
    QString diskWriteRate() const { return m_diskWriteRate; }
    qulonglong diskReadBytesSec() const { return m_diskReadBytesSec; }
    qulonglong diskWriteBytesSec() const { return m_diskWriteBytesSec; }
    QVariantList diskHistory() const { return m_diskHistory; }

    QString netRxRate() const { return m_netRxRate; }
    QString netTxRate() const { return m_netTxRate; }
    qulonglong netRxBytesSec() const { return m_netRxBytesSec; }
    qulonglong netTxBytesSec() const { return m_netTxBytesSec; }
    QVariantList netHistory() const { return m_netHistory; }

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

    QVariantList services() const { return m_services; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectProcess(int pid);
    Q_INVOKABLE void toggleSort(const QString& col);
    Q_INVOKABLE void openKillDialog(bool force);
    Q_INVOKABLE void closeKillDialog();
    Q_INVOKABLE bool confirmKillProcess();
    Q_INVOKABLE bool restartService(const QString& name);

signals:
    void telemetryChanged();
    void historyChanged();
    void processesChanged();
    void filterChanged();
    void selectionChanged();
    void killModalChanged();
    void servicesChanged();

private:
    void updateTelemetry(const SystemSnapshot& snap);
    void updateProcessesList();
    void updateServicesList();
    static QString formatBytes(uint64_t bytes);
    static QString formatRate(uint64_t bytesSec);

    qreal m_cpuUsage{0.0};
    qreal m_cpuTemperature{45.0};
    int m_cpuCoresCount{1};
    QString m_cpuModel;
    QVariantList m_cpuHistory;

    qulonglong m_memUsedBytes{0};
    qulonglong m_memTotalBytes{0};
    QString m_memUsedGb{"0.0 GB"};
    QString m_memTotalGb{"0.0 GB"};
    qreal m_memUsagePct{0.0};
    QVariantList m_memHistory;

    QString m_diskReadRate{"0 KB/s"};
    QString m_diskWriteRate{"0 KB/s"};
    qulonglong m_diskReadBytesSec{0};
    qulonglong m_diskWriteBytesSec{0};
    QVariantList m_diskHistory;

    QString m_netRxRate{"0 KB/s"};
    QString m_netTxRate{"0 KB/s"};
    qulonglong m_netRxBytesSec{0};
    qulonglong m_netTxBytesSec{0};
    QVariantList m_netHistory;

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

    QVariantList m_services;

    uid_t m_currentUid{0};
    std::vector<tinexus::platform::ServiceInfo> m_cachedServices;
};

} // namespace tinexus::monitor
