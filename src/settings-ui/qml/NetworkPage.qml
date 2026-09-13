import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    signal requestWifiPassword(string ssid)

    ColumnLayout {
        width: Math.min(parent.width - 48, 680)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14

        Item { height: 4 }

        // ── 1. Master Wi-Fi Banner Card ───────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: 74

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 14

                // Large Wi-Fi Squircle Icon
                MacSquircleIcon {
                    iconSize: 36
                    glyphSize: 18
                    iconColor: "#0A84FF"
                    iconId: "wifi"
                }

                ColumnLayout {
                    spacing: 1
                    Layout.fillWidth: true

                    Text {
                        text: "Wi-Fi"
                        color: "#f5f5f7"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: "Set up Wi-Fi to wirelessly connect your computer to the internet. Turn on Wi-Fi, then choose a network to join."
                        color: "#86868b"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }

                MacSwitch {
                    checked: bridge.wifiEnabled
                    onToggled: (val) => bridge.setWifiEnabled(val)
                }
            }
        }

        // ── 2. Connected Network ──────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: bridge.wifiEnabled && bridge.connectedSsid.length > 0

            Text {
                text: "CURRENT CONNECTION"
                color: "#86868b"
                font.pixelSize: 11
                font.weight: Font.Medium
                Layout.leftMargin: 6
            }

            MacCard {
                Layout.fillWidth: true
                implicitHeight: 52

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12

                    // Green status dot
                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: "#30d158"
                    }

                    ColumnLayout {
                        spacing: 1
                        Layout.fillWidth: true

                        Text {
                            text: bridge.connectedSsid
                            color: "#f5f5f7"
                            font.pixelSize: 13
                            font.weight: Font.Medium
                        }

                        Text {
                            text: "Connected  •  " + (bridge.ipAddress.length > 0 ? bridge.ipAddress : "Active")
                            color: "#86868b"
                            font.pixelSize: 11
                        }
                    }

                    // Lock icon (vector)
                    Item {
                        width: 12; height: 14
                        Rectangle {
                            x: 2; y: 6; width: 8; height: 8; radius: 1.5
                            color: "#86868b"
                        }
                        Rectangle {
                            x: 4; y: 1; width: 4; height: 6; radius: 2
                            color: "transparent"
                            border.color: "#86868b"; border.width: 1.5
                        }
                    }

                    // Wi-Fi signal bars (vector)
                    Canvas {
                        width: 18; height: 14
                        property int bars: bridge.connectedSignalBars > 0 ? bridge.connectedSignalBars : 4
                        onBarsChanged: requestPaint()
                        Component.onCompleted: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var cx = width / 2, cy = height - 2
                            var radii = [4, 6.5, 9, 11.5]
                            for (var i = 0; i < 4; i++) {
                                ctx.beginPath()
                                ctx.strokeStyle = i < bars ? "#f5f5f7" : "#48484c"
                                ctx.lineWidth = 1.5
                                ctx.arc(cx, cy, radii[i], Math.PI * 1.25, Math.PI * 1.75, false)
                                ctx.stroke()
                            }
                            ctx.beginPath()
                            ctx.fillStyle = "#f5f5f7"
                            ctx.arc(cx, cy, 1.5, 0, 2 * Math.PI)
                            ctx.fill()
                        }
                    }

                    MacButton {
                        text: "Details..."
                        onClicked: detailsPopup.open()
                    }

                    MacButton {
                        text: "Disconnect"
                        onClicked: bridge.disconnectWifi()
                    }
                }
            }
        }

        // ── 3. Known Networks Section ─────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: bridge.wifiEnabled && bridge.connectedSsid.length > 0

            Text {
                text: "KNOWN NETWORKS"
                color: "#86868b"
                font.pixelSize: 11
                font.weight: Font.Medium
                Layout.leftMargin: 6
            }

            MacCard {
                Layout.fillWidth: true
                implicitHeight: 44

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 10

                    Text {
                        text: "✓"
                        color: "#f5f5f7"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Text {
                        text: bridge.connectedSsid
                        color: "#f5f5f7"
                        font.pixelSize: 13
                        font.weight: Font.Normal
                        Layout.fillWidth: true
                    }

                    // Lock icon (vector)
                    Item {
                        width: 12; height: 14
                        Rectangle {
                            x: 2; y: 6; width: 8; height: 8; radius: 1.5
                            color: "#86868b"
                        }
                        Rectangle {
                            x: 4; y: 1; width: 4; height: 6; radius: 2
                            color: "transparent"
                            border.color: "#86868b"; border.width: 1.5
                        }
                    }

                    // Wi-Fi signal bars (vector)
                    Canvas {
                        width: 18; height: 14
                        property int bars: bridge.connectedSignalBars > 0 ? bridge.connectedSignalBars : 4
                        onBarsChanged: requestPaint()
                        Component.onCompleted: requestPaint()
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.clearRect(0, 0, width, height)
                            var cx = width / 2, cy = height - 2
                            var radii = [4, 6.5, 9, 11.5]
                            for (var i = 0; i < 4; i++) {
                                ctx.beginPath()
                                ctx.strokeStyle = i < bars ? "#f5f5f7" : "#48484c"
                                ctx.lineWidth = 1.5
                                ctx.arc(cx, cy, radii[i], Math.PI * 1.25, Math.PI * 1.75, false)
                                ctx.stroke()
                            }
                            ctx.beginPath()
                            ctx.fillStyle = "#f5f5f7"
                            ctx.arc(cx, cy, 1.5, 0, 2 * Math.PI)
                            ctx.fill()
                        }
                    }

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 11
                        color: optArea.containsMouse ? "#18ffffff" : "transparent"
                        border.color: "#24ffffff"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "···"
                            color: "#86868b"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            id: optArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: detailsPopup.open()
                        }
                    }
                }
            }
        }

        // ── 4. Other Networks Section ─────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: bridge.wifiEnabled

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 6
                Layout.rightMargin: 6

                Text {
                    text: "OTHER NETWORKS"
                    color: "#86868b"
                    font.pixelSize: 11
                    font.weight: Font.Medium
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: bridge.isScanning ? "Scanning..." : "Scan"
                    color: "#0A84FF"
                    font.pixelSize: 11
                    font.weight: Font.Medium

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: bridge.triggerWifiScan()
                    }
                }
            }

            MacCard {
                Layout.fillWidth: true
                implicitHeight: networkListCol.implicitHeight + 8

                ColumnLayout {
                    id: networkListCol
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 0

                    Repeater {
                        model: bridge.wifiNetworks

                        delegate: ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                height: 40
                                radius: 6
                                color: rowHoverArea.containsMouse ? "#0cffffff" : "transparent"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    Text {
                                        text: modelData.ssid.length > 0 ? modelData.ssid : "<Hidden Network>"
                                        color: "#f5f5f7"
                                        font.pixelSize: 13
                                        font.weight: Font.Normal
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    MacButton {
                                        text: modelData.connected ? "Connected" : "Connect"
                                        primary: false
                                        visible: rowHoverArea.containsMouse || modelData.connected
                                        enabled: !modelData.connected
                                        onClicked: {
                                            if (modelData.secured) {
                                                root.requestWifiPassword(modelData.ssid)
                                            } else {
                                                bridge.connectWifi(modelData.ssid, "")
                                            }
                                        }
                                    }

                                    // Lock icon (vector) — shown for secured networks
                                    Item {
                                        width: 11; height: 13
                                        visible: modelData.secured
                                        Rectangle {
                                            x: 2; y: 5; width: 7; height: 7; radius: 1.5
                                            color: "#86868b"
                                        }
                                        Rectangle {
                                            x: 3.5; y: 0.5; width: 4; height: 5.5; radius: 2
                                            color: "transparent"
                                            border.color: "#86868b"; border.width: 1.5
                                        }
                                    }

                                    Row {
                                        spacing: 2
                                        Repeater {
                                            model: 4
                                            Rectangle {
                                                width: 3
                                                height: 4 + index * 3
                                                radius: 1
                                                color: index < modelData.bars ? "#f5f5f7" : "#48484c"
                                                anchors.bottom: parent.bottom
                                            }
                                        }
                                    }

                                    Rectangle {
                                        width: 20
                                        height: 20
                                        radius: 10
                                        color: "transparent"
                                        border.color: "#1cffffff"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: "···"
                                            color: "#86868b"
                                            font.pixelSize: 11
                                            font.bold: true
                                        }
                                    }
                                }

                                MouseArea {
                                    id: rowHoverArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        if (modelData.secured) {
                                            root.requestWifiPassword(modelData.ssid)
                                        } else {
                                            bridge.connectWifi(modelData.ssid, "")
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.leftMargin: 12
                                Layout.rightMargin: 12
                                height: 1
                                color: "#12ffffff"
                                visible: index < bridge.wifiNetworks.length - 1
                            }
                        }
                    }

                    // Empty scan state
                    Text {
                        text: "No wireless networks detected nearby."
                        color: "#86868b"
                        font.pixelSize: 12
                        visible: bridge.wifiNetworks.length === 0
                        Layout.alignment: Qt.AlignHCenter
                        Layout.margins: 14
                    }

                    // Bottom "Other..." row
                    Item {
                        Layout.fillWidth: true
                        height: 36

                        RowLayout {
                            anchors.fill: parent
                            anchors.rightMargin: 10

                            Item { Layout.fillWidth: true }

                            MacButton {
                                text: "Other..."
                                onClicked: root.requestWifiPassword("")
                            }
                        }
                    }
                }
            }
        }

        // ── 5. Ask to Join Networks ───────────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: askCol.implicitHeight + 20

            ColumnLayout {
                id: askCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Ask to join networks"
                        color: "#f5f5f7"
                        font.pixelSize: 13
                        font.weight: Font.Normal
                    }

                    Item { Layout.fillWidth: true }

                    MacComboBox {
                        model: ["Notify", "Ask", "Off"]
                        currentIndex: 0
                    }
                }

                Text {
                    text: "Known networks will be joined automatically. If no known networks are available, you will be notified of available networks."
                    color: "#86868b"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }

        // Section Title: Physical Adapters
        Text {
            text: "PHYSICAL NETWORK ADAPTERS"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── 6. Physical Network Adapters ──────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: adapterCol.implicitHeight + 16

            ColumnLayout {
                id: adapterCol
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: "Hardware Interfaces"
                        color: "#f5f5f7"
                        font.pixelSize: 13
                        font.weight: Font.Normal
                    }

                    Item { Layout.fillWidth: true }

                    MacButton {
                        text: "Refresh"
                        onClicked: bridge.refreshNetworkInterfaces()
                    }
                }

                Repeater {
                    model: bridge.networkInterfaces
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 36
                        radius: 6
                        color: "#1e1e20"
                        border.color: "#10ffffff"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10

                            Text {
                                text: modelData.name
                                color: "#f5f5f7"
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }

                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: modelData.state === "up" ? "#30d158" : "#ff453a"
                            }

                            Text {
                                text: modelData.state.toUpperCase()
                                color: modelData.state === "up" ? "#30d158" : "#ff453a"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: modelData.ip4.length > 0 ? ("IPv4: " + modelData.ip4) : "No IPv4 address"
                                color: "#86868b"
                                font.pixelSize: 11
                                font.family: "monospace"
                            }
                        }
                    }
                }
            }
        }

        Item { height: 10 }
    }

    // ── Details Dialog Popup ───────────────────────────────────────────────
    Popup {
        id: detailsPopup
        anchors.centerIn: parent
        width: 360
        height: 240
        modal: true
        focus: true

        background: Rectangle {
            color: "#28282a"
            radius: 12
            border.color: "#20ffffff"
            border.width: 1
        }

        contentItem: ColumnLayout {
            anchors.margins: 16
            spacing: 12

            Text {
                text: "Network Details"
                color: "#f5f5f7"
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#14ffffff" }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Network Name (SSID):"; color: "#86868b"; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Text { text: bridge.connectedSsid; color: "#f5f5f7"; font.pixelSize: 12; font.weight: Font.Medium }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "IPv4 Address:"; color: "#86868b"; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Text {
                    text: bridge.ipAddress.length > 0 ? bridge.ipAddress : (bridge.connectedSsid.length > 0 ? "Acquiring IPv4..." : "Not Connected")
                    color: "#f5f5f7"
                    font.pixelSize: 12
                    font.family: "monospace"
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Hardware Interface:"; color: "#86868b"; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                Text {
                    text: (bridge.activeInterface && bridge.activeInterface.length > 0) ? bridge.activeInterface : "Disconnected"
                    color: "#f5f5f7"
                    font.pixelSize: 12
                    font.family: "monospace"
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                MacButton {
                    text: "Done"
                    primary: true
                    onClicked: detailsPopup.close()
                }
            }
        }
    }
}
