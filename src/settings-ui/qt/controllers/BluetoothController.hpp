#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <atomic>
#include <memory>

namespace tinexus::settings_ui {

class BluetoothController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool bluetoothAvailable READ bluetoothAvailable NOTIFY bluetoothStateChanged)
    Q_PROPERTY(bool bluetoothEnabled READ bluetoothEnabled WRITE setBluetoothEnabled NOTIFY bluetoothStateChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY bluetoothStateChanged)
    Q_PROPERTY(QString adapterName READ adapterName NOTIFY bluetoothStateChanged)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY devicesChanged)

public:
    explicit BluetoothController(QObject* parent = nullptr);
    ~BluetoothController() override = default;

    bool bluetoothAvailable() const { return m_available; }
    bool bluetoothEnabled() const { return m_enabled; }
    bool isScanning() const { return m_scanning; }
    QString adapterName() const { return m_adapterName; }
    QVariantList devices() const { return m_devices; }

public slots:
    void refreshDevices();
    void setBluetoothEnabled(bool enabled);
    void startDiscovery();
    void stopDiscovery();
    void pairAndConnect(const QString& address);
    void disconnectDevice(const QString& address);
    void removeDevice(const QString& address);

signals:
    void bluetoothStateChanged();
    void devicesChanged();
    void toastRequested(const QString& message, bool isError);

private:
    bool m_available{false};
    bool m_enabled{false};
    bool m_scanning{false};
    QString m_adapterName{"Bluetooth Adapter"};
    QString m_adapterPath;
    QVariantList m_devices;
    std::shared_ptr<std::atomic<bool>> m_busy{std::make_shared<std::atomic<bool>>(false)};
};

} // namespace tinexus::settings_ui
