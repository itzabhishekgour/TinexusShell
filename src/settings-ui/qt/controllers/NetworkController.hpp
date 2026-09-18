#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>

namespace tinexus::settings_ui {

class NetworkController : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool wifiEnabled READ wifiEnabled WRITE setWifiEnabled NOTIFY wifiEnabledChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(bool isConnecting READ isConnecting NOTIFY isConnectingChanged)
    Q_PROPERTY(QString connectedSsid READ connectedSsid NOTIFY connectedSsidChanged)
    Q_PROPERTY(QString connectingSsid READ connectingSsid NOTIFY connectingSsidChanged)
    Q_PROPERTY(QString ipAddress READ ipAddress NOTIFY ipAddressChanged)
    Q_PROPERTY(QString activeInterface READ activeInterface NOTIFY activeInterfaceChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int connectedSignalBars READ connectedSignalBars NOTIFY connectedSignalBarsChanged)
    Q_PROPERTY(QVariantList networkInterfaces READ networkInterfaces NOTIFY networkInterfacesChanged)
    Q_PROPERTY(QVariantList wifiNetworks READ wifiNetworks NOTIFY wifiNetworksChanged)

public:
    explicit NetworkController(QObject* parent = nullptr);
    ~NetworkController() override = default;

    bool wifiEnabled() const;
    bool isScanning() const;
    bool isConnecting() const;
    QString connectedSsid() const;
    QString connectingSsid() const;
    QString ipAddress() const;
    QString activeInterface() const;
    QString statusMessage() const;
    int connectedSignalBars() const;
    QVariantList networkInterfaces() const { return m_networkInterfaces; }
    QVariantList wifiNetworks() const;

public slots:
    void setWifiEnabled(bool enabled);
    void triggerWifiScan();
    void connectWifi(const QString& ssid, const QString& password);
    void disconnectWifi();
    void refreshNetworkInterfaces();
    void pollWifiStatus();

signals:
    void wifiEnabledChanged();
    void isScanningChanged();
    void isConnectingChanged();
    void connectedSsidChanged();
    void connectingSsidChanged();
    void ipAddressChanged();
    void activeInterfaceChanged();
    void statusMessageChanged();
    void connectedSignalBarsChanged();
    void networkInterfacesChanged();
    void wifiNetworksChanged();
    void toastRequested(const QString& message, bool isError);
    void settingModified(const QString& key, const QVariant& value);

private:
    QVariantList m_networkInterfaces;
};

} // namespace tinexus::settings_ui
