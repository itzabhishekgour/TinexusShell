#include "NetworkController.hpp"
#include "settings/WifiManager.hpp"
#include <common/NetUtils.hpp>
#include <common/logger.hpp>

namespace tinexus::settings_ui {

NetworkController::NetworkController(QObject* parent)
    : QObject(parent)
{
    refreshNetworkInterfaces();
}

bool NetworkController::wifiEnabled() const {
    return WifiManager::instance().is_wifi_enabled();
}

bool NetworkController::isScanning() const {
    return WifiManager::instance().is_scanning();
}

bool NetworkController::isConnecting() const {
    return WifiManager::instance().is_connecting();
}

QString NetworkController::connectedSsid() const {
    return QString::fromStdString(WifiManager::instance().get_connected_ssid());
}

QString NetworkController::connectingSsid() const {
    return QString::fromStdString(WifiManager::instance().get_connecting_ssid());
}

QString NetworkController::ipAddress() const {
    return QString::fromStdString(WifiManager::instance().get_ip_address());
}

QString NetworkController::activeInterface() const {
    return QString::fromStdString(WifiManager::instance().get_active_interface());
}

QString NetworkController::statusMessage() const {
    return QString::fromStdString(WifiManager::instance().get_status_message());
}

int NetworkController::connectedSignalBars() const {
    return WifiManager::instance().get_connected_signal_bars();
}

QVariantList NetworkController::wifiNetworks() const {
    QVariantList list;
    auto nets = WifiManager::instance().get_networks();
    for (const auto& n : nets) {
        QVariantMap map;
        map["ssid"] = QString::fromStdString(n.ssid);
        map["bssid"] = QString::fromStdString(n.bssid);
        map["signalDbm"] = n.signal_dbm;
        map["bars"] = n.signal_bars;
        map["secured"] = n.is_secured;
        map["securityType"] = QString::fromStdString(n.security_str);
        map["connected"] = n.is_connected;
        list.append(map);
    }
    return list;
}

void NetworkController::setWifiEnabled(bool enabled) {
    WifiManager::instance().set_wifi_enabled(enabled);
    emit wifiEnabledChanged();
    emit wifiNetworksChanged();
    emit toastRequested(enabled ? QStringLiteral("Wi-Fi subsystem enabled") : QStringLiteral("Wi-Fi subsystem disabled"), false);
    emit settingModified(QStringLiteral("wifi_enabled"), enabled);
}

void NetworkController::triggerWifiScan() {
    WifiManager::instance().trigger_scan();
    emit isScanningChanged();
    emit toastRequested(QStringLiteral("Scanning for available Wi-Fi networks..."), false);
}

void NetworkController::connectWifi(const QString& ssid, const QString& password) {
    WifiManager::instance().connect(ssid.toStdString(), password.toStdString());
    emit isConnectingChanged();
    emit connectingSsidChanged();
    emit toastRequested(QStringLiteral("Connecting to \"%1\"...").arg(ssid), false);
}

void NetworkController::disconnectWifi() {
    WifiManager::instance().disconnect();
    emit connectedSsidChanged();
    emit ipAddressChanged();
    emit activeInterfaceChanged();
    emit toastRequested(QStringLiteral("Disconnected from Wi-Fi"), false);
}

void NetworkController::refreshNetworkInterfaces() {
    m_networkInterfaces.clear();
    auto phys = tinexus::net::get_physical_interfaces("/sys/class/net");
    for (const auto& p : phys) {
        QVariantMap iface;
        iface["name"] = QString::fromStdString(p.name);
        iface["ip4"] = QString::fromStdString(p.ip4_addr);
        iface["state"] = QString::fromStdString(p.operstate);
        iface["rx"] = static_cast<qulonglong>(p.rx_mb);
        iface["tx"] = static_cast<qulonglong>(p.tx_mb);
        m_networkInterfaces.append(iface);
    }
    emit networkInterfacesChanged();
}

void NetworkController::pollWifiStatus() {
    emit wifiNetworksChanged();
    emit isScanningChanged();
    emit isConnectingChanged();
    emit connectedSsidChanged();
    emit ipAddressChanged();
    emit activeInterfaceChanged();
    emit statusMessageChanged();
    emit connectedSignalBarsChanged();
}

} // namespace tinexus::settings_ui
