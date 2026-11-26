import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: Math.min(parent.width - 48, 680)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14

        Item { height: 4 }

        // ── Bluetooth Banner ──────────────────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: 74

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 14

                Image {
                    width: 34; height: 34
                    source: "image://tinexus/bluetooth"
                    sourceSize: Qt.size(34, 34)
                }

                ColumnLayout {
                    spacing: 2
                    Layout.fillWidth: true

                    Text {
                        text: "Bluetooth"
                        color: "#f5f5f7"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: !bridge.bluetoothAvailable
                              ? "No Bluetooth controller detected on this hardware."
                              : (bridge.bluetoothEnabled ? ("Discoverable as " + bridge.bluetoothAdapterName) : "Bluetooth is turned off.")
                        color: "#86868b"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                MacSwitch {
                    checked: bridge.bluetoothEnabled
                    enabled: bridge.bluetoothAvailable
                    onToggled: (val) => bridge.setBluetoothEnabled(val)
                }
            }
        }

        // ── Scan Controls ─────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            visible: bridge.bluetoothAvailable && bridge.bluetoothEnabled

            Text {
                text: "MY DEVICES & NEARBY"
                color: "#86868b"
                font.pixelSize: 11
                font.weight: Font.Medium
                Layout.leftMargin: 6
                Layout.fillWidth: true
            }

            MacButton {
                text: bridge.bluetoothScanning ? "Scanning..." : "Scan Devices"
                enabled: !bridge.bluetoothScanning
                onClicked: bridge.startBluetoothDiscovery()
            }

            MacButton {
                text: "Refresh"
                onClicked: bridge.refreshBluetoothDevices()
            }
        }

        // ── Devices Card ──────────────────────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            visible: bridge.bluetoothAvailable && bridge.bluetoothEnabled
            implicitHeight: btCol.implicitHeight

            ColumnLayout {
                id: btCol
                anchors.fill: parent
                spacing: 0

                Text {
                    visible: bridge.bluetoothDevices.length === 0
                    text: "No paired or nearby Bluetooth devices found. Click Scan Devices to search."
                    color: "#86868b"
                    font.pixelSize: 12
                    Layout.margins: 16
                }

                Repeater {
                    model: bridge.bluetoothDevices
                    delegate: MacSettingRow {
                        title: modelData.name || modelData.address
                        subtitle: (modelData.connected ? "Connected" : (modelData.paired ? "Paired • Not Connected" : "Available")) + "  •  " + modelData.address
                        showSeparator: index < bridge.bluetoothDevices.length - 1

                        RowLayout {
                            spacing: 6

                            MacButton {
                                text: modelData.connected ? "Disconnect" : (modelData.paired ? "Connect" : "Pair")
                                primary: !modelData.connected
                                onClicked: {
                                    if (modelData.connected) bridge.disconnectBluetoothDevice(modelData.address)
                                    else bridge.pairBluetoothDevice(modelData.address)
                                }
                            }

                            MacButton {
                                visible: modelData.paired
                                text: "Forget"
                                destructive: true
                                onClicked: bridge.removeBluetoothDevice(modelData.address)
                            }
                        }
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
