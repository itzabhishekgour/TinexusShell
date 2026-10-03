#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace tinexus::about {

class AboutBridge : public QObject {
    Q_OBJECT

    // Overview Tab properties
    Q_PROPERTY(QString osTitle READ osTitle NOTIFY systemInfoChanged)
    Q_PROPERTY(QString osVersion READ osVersion NOTIFY systemInfoChanged)
    Q_PROPERTY(QString kernelVersion READ kernelVersion NOTIFY systemInfoChanged)
    Q_PROPERTY(QString platformInfo READ platformInfo NOTIFY systemInfoChanged)
    Q_PROPERTY(QString cpuModel READ cpuModel NOTIFY systemInfoChanged)
    Q_PROPERTY(QString memInfo READ memInfo NOTIFY systemInfoChanged)
    Q_PROPERTY(QString graphicsInfo READ graphicsInfo NOTIFY systemInfoChanged)
    Q_PROPERTY(QString rootDevice READ rootDevice NOTIFY systemInfoChanged)

    // Displays Tab properties
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayInfoChanged)
    Q_PROPERTY(QString displayResolution READ displayResolution NOTIFY displayInfoChanged)
    Q_PROPERTY(QString displayRefreshRate READ displayRefreshRate NOTIFY displayInfoChanged)
    Q_PROPERTY(QString displayScale READ displayScale NOTIFY displayInfoChanged)
    Q_PROPERTY(QString displayColorFormat READ displayColorFormat NOTIFY displayInfoChanged)
    Q_PROPERTY(QString displayRenderer READ displayRenderer NOTIFY displayInfoChanged)

    // Storage Tab properties
    Q_PROPERTY(QString storageMount READ storageMount NOTIFY storageInfoChanged)
    Q_PROPERTY(QString storageDevice READ storageDevice NOTIFY storageInfoChanged)
    Q_PROPERTY(QString storageFsType READ storageFsType NOTIFY storageInfoChanged)
    Q_PROPERTY(qreal storageTotalGb READ storageTotalGb NOTIFY storageInfoChanged)
    Q_PROPERTY(qreal storageUsedGb READ storageUsedGb NOTIFY storageInfoChanged)
    Q_PROPERTY(qreal storageFreeGb READ storageFreeGb NOTIFY storageInfoChanged)
    Q_PROPERTY(qreal storageUsedPct READ storageUsedPct NOTIFY storageInfoChanged)

    // Services Tab properties
    Q_PROPERTY(QVariantList services READ services NOTIFY servicesChanged)

public:
    explicit AboutBridge(QObject* parent = nullptr);
    ~AboutBridge() override = default;

    QString osTitle() const { return m_osTitle; }
    QString osVersion() const { return m_osVersion; }
    QString kernelVersion() const { return m_kernelVersion; }
    QString platformInfo() const { return m_platformInfo; }
    QString cpuModel() const { return m_cpuModel; }
    QString memInfo() const { return m_memInfo; }
    QString graphicsInfo() const { return m_graphicsInfo; }
    QString rootDevice() const { return m_rootDevice; }

    QString displayName() const { return m_displayName; }
    QString displayResolution() const { return m_displayResolution; }
    QString displayRefreshRate() const { return m_displayRefreshRate; }
    QString displayScale() const { return m_displayScale; }
    QString displayColorFormat() const { return m_displayColorFormat; }
    QString displayRenderer() const { return m_displayRenderer; }

    QString storageMount() const { return m_storageMount; }
    QString storageDevice() const { return m_storageDevice; }
    QString storageFsType() const { return m_storageFsType; }
    qreal storageTotalGb() const { return m_storageTotalGb; }
    qreal storageUsedGb() const { return m_storageUsedGb; }
    qreal storageFreeGb() const { return m_storageFreeGb; }
    qreal storageUsedPct() const { return m_storageUsedPct; }

    QVariantList services() const { return m_services; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void launchSystemReport();
    Q_INVOKABLE void launchSoftwareUpdate();

signals:
    void systemInfoChanged();
    void displayInfoChanged();
    void storageInfoChanged();
    void servicesChanged();

private:
    void readSystemInfo();
    void readDisplayInfo();
    void readStorageInfo();
    void readServicesInfo();

    QString m_osTitle{"Tinexus Desktop"};
    QString m_osVersion{"Version 1.0 (Architecture Freeze - LTS)"};
    QString m_kernelVersion;
    QString m_platformInfo{"Wayland Native / Pure C++20"};
    QString m_cpuModel;
    QString m_memInfo;
    QString m_graphicsInfo;
    QString m_rootDevice;

    QString m_displayName;
    QString m_displayResolution;
    QString m_displayRefreshRate;
    QString m_displayScale{"100% (Native 1:1 Pixel Grid)"};
    QString m_displayColorFormat{"32-bit ARGB8888 (sRGB D65)"};
    QString m_displayRenderer;

    QString m_storageMount{"/"};
    QString m_storageDevice{"Root NVMe SSD"};
    QString m_storageFsType{"ext4 / OverlayFS"};
    qreal m_storageTotalGb{64.0};
    qreal m_storageUsedGb{16.0};
    qreal m_storageFreeGb{48.0};
    qreal m_storageUsedPct{0.25};

    QVariantList m_services;
};

} // namespace tinexus::about
