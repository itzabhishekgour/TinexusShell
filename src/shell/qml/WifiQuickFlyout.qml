// ============================================================================
// WifiQuickFlyout.qml — Inline Topbar Wi-Fi Quick Controls with LiquidGlass
// ============================================================================
import QtQuick
import QtQuick.Layouts
import "../../common/qml"

Item {
    id: root
    readonly property int shadowPad: 18
    readonly property int cardWidth: 300
    readonly property int listAreaHeight: {
        if (!bridge || !bridge.wifiEnabled) return 76
        var count = bridge.wifiNetworks ? bridge.wifiNetworks.length : 0
        if (count === 0) return 76
        var rowsH = Math.min(212, count * 44)
        return passwordTargetSsid.length > 0 ? Math.min(264, rowsH + 56) : rowsH
    }
    readonly property int cardHeight: 112 + listAreaHeight

    property string passwordTargetSsid: ""
    property string statusHint: ""

    width: cardWidth + shadowPad * 2
    height: cardHeight + shadowPad * 2

    Connections {
        target: typeof bridge !== "undefined" ? bridge : null
        function onWifiFlyoutOpenChanged() {
            if (!bridge.wifiFlyoutOpen) {
                root.passwordTargetSsid = ""
                passwordInput.text = ""
            }
        }
    }

    Item {
        id: glassCard
        x: root.shadowPad
        y: root.shadowPad
        width: root.cardWidth
        height: root.cardHeight

        LiquidGlass {
            anchors.fill: parent
            materialType: "popover"
            cornerRadius: 14
            blurAmount: 28
            fluidOpacity: 0.68
            shadowEnabled: true
            topSpecularOpacity: 0.14
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            // ── Header: Title + Scan + Power Switch ───────────────────
            RowLayout {
                Layout.fillWidth: true
                height: 26
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: "Wi-Fi"
                        color: "#FFFFFF"
                        font.family: "Inter"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        text: typeof bridge !== "undefined"
                              ? (bridge.wifiScanning ? "Scanning networks..." : bridge.wifiStatusMessage)
                              : "Not connected"
                        color: Qt.rgba(1, 1, 1, 0.58)
                        font.family: "Inter"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // Refresh / Rescan button
                Rectangle {
                    width: 24
                    height: 24
                    radius: 6
                    visible: typeof bridge !== "undefined" && bridge.wifiEnabled
                    color: rescanMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        fillMode: Image.PreserveAspectFit
                        opacity: (typeof bridge !== "undefined" && bridge.wifiScanning) ? 0.5 : 0.9
                        source: "image://icon/view-refresh-symbolic?color=#FFFFFF"
                    }

                    MouseArea {
                        id: rescanMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") bridge.scanWifiNetworks()
                        }
                    }
                }

                // Tactile Spring Toggle Switch
                Rectangle {
                    id: wifiSwitch
                    readonly property bool checked: typeof bridge !== "undefined" ? bridge.wifiEnabled : false
                    width: 38
                    height: 22
                    radius: 11
                    color: checked ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.18)
                    Behavior on color { ColorAnimation { duration: 150; easing.type: Easing.OutCubic } }

                    Rectangle {
                        width: 18
                        height: 18
                        radius: 9
                        y: 2
                        x: wifiSwitch.checked ? (parent.width - width - 2) : 2
                        color: "#FFFFFF"
                        Behavior on x {
                            SpringAnimation { spring: 6.0; damping: 0.72; epsilon: 0.25 }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") {
                                root.passwordTargetSsid = ""
                                bridge.setWifiEnabled(!bridge.wifiEnabled)
                            }
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // ── Wi-Fi Off / Empty State ───────────────────────────────
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: root.listAreaHeight
                visible: typeof bridge === "undefined" || !bridge.wifiEnabled || !bridge.wifiNetworks || bridge.wifiNetworks.length === 0

                Column {
                    anchors.centerIn: parent
                    spacing: 4

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: (typeof bridge !== "undefined" && !bridge.wifiEnabled)
                              ? "Wi-Fi is turned off"
                              : ((typeof bridge !== "undefined" && bridge.wifiScanning) ? "Searching for Wi-Fi networks..." : "No networks in range")
                        color: Qt.rgba(1, 1, 1, 0.65)
                        font.family: "Inter"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                }
            }

            // ── Nearby Networks List ──────────────────────────────────
            ListView {
                id: apListView
                Layout.fillWidth: true
                Layout.preferredHeight: root.listAreaHeight
                clip: true
                spacing: 4
                visible: typeof bridge !== "undefined" && bridge.wifiEnabled && bridge.wifiNetworks && bridge.wifiNetworks.length > 0
                model: typeof bridge !== "undefined" ? bridge.wifiNetworks : []

                delegate: Column {
                    width: apListView.width
                    spacing: 4

                    readonly property bool isConnected: modelData.connected === true
                    readonly property bool isPrompting: root.passwordTargetSsid === modelData.ssid
                    readonly property string sigIcon: {
                        var b = modelData.bars || 1
                        if (b >= 4) return "network-wireless-signal-excellent-symbolic"
                        if (b === 3) return "network-wireless-signal-good-symbolic"
                        if (b === 2) return "network-wireless-signal-ok-symbolic"
                        return "network-wireless-signal-weak-symbolic"
                    }

                    Rectangle {
                        width: parent.width
                        height: 40
                        radius: 9
                        color: isConnected
                               ? Qt.rgba(10 / 255.0, 132 / 255.0, 255 / 255.0, 0.22)
                               : (apMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.11) : Qt.rgba(1, 1, 1, 0.04))
                        border.color: isConnected ? Qt.rgba(10 / 255.0, 132 / 255.0, 255 / 255.0, 0.45) : "transparent"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 9

                            Image {
                                Layout.preferredWidth: 16
                                Layout.preferredHeight: 16
                                sourceSize.width: 16
                                sourceSize.height: 16
                                fillMode: Image.PreserveAspectFit
                                source: "image://icon/" + sigIcon + "?color=" + (isConnected ? "#38BDF8" : "#FFFFFF")
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0

                                Text {
                                    text: modelData.ssid || ""
                                    color: "#FFFFFF"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: isConnected ? Font.DemiBold : Font.Medium
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: isConnected
                                          ? ((bridge.wifiIpAddress && bridge.wifiIpAddress.length > 0) ? bridge.wifiIpAddress : "Connected")
                                          : (modelData.security || "Open")
                                    color: isConnected ? "#64D2FF" : Qt.rgba(1, 1, 1, 0.48)
                                    font.family: "Inter"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            // Lock icon for secured networks when not connected
                            Image {
                                Layout.preferredWidth: 12
                                Layout.preferredHeight: 12
                                sourceSize.width: 12
                                sourceSize.height: 12
                                visible: !isConnected && modelData.secured === true
                                opacity: 0.65
                                fillMode: Image.PreserveAspectFit
                                source: "image://icon/changes-prevent-symbolic?color=#FFFFFF"
                            }

                            // Connected Checkmark + Disconnect pill
                            Rectangle {
                                visible: isConnected
                                Layout.preferredWidth: disTxt.implicitWidth + 14
                                Layout.preferredHeight: 22
                                radius: 6
                                color: disMouse.containsMouse ? Qt.rgba(255/255.0, 69/255.0, 58/255.0, 0.25) : Qt.rgba(1, 1, 1, 0.12)

                                Text {
                                    id: disTxt
                                    anchors.centerIn: parent
                                    text: disMouse.containsMouse ? "Disconnect" : "✓"
                                    color: disMouse.containsMouse ? "#FF6961" : "#38BDF8"
                                    font.family: "Inter"
                                    font.pixelSize: disMouse.containsMouse ? 10 : 12
                                    font.bold: true
                                }

                                MouseArea {
                                    id: disMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (typeof bridge !== "undefined") bridge.disconnectWifi()
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: apMouse
                            anchors.fill: parent
                            anchors.rightMargin: isConnected ? 80 : 0
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge === "undefined") return
                                if (isConnected) return
                                if (modelData.secured === true) {
                                    if (root.passwordTargetSsid === modelData.ssid) {
                                        root.passwordTargetSsid = ""
                                    } else {
                                        root.passwordTargetSsid = modelData.ssid
                                        passwordInput.text = ""
                                        passwordInput.forceActiveFocus()
                                    }
                                } else {
                                    root.passwordTargetSsid = ""
                                    bridge.connectWifi(modelData.ssid, "")
                                }
                            }
                        }
                    }

                    // Inline Password Entry Box for Secured AP
                    Rectangle {
                        width: parent.width
                        height: 44
                        radius: 8
                        visible: isPrompting
                        color: Qt.rgba(0, 0, 0, 0.36)
                        border.color: Qt.rgba(10 / 255.0, 132 / 255.0, 255 / 255.0, 0.55)
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 6
                            spacing: 6

                            TextInput {
                                id: passwordInput
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                verticalAlignment: TextInput.AlignVCenter
                                echoMode: TextInput.Password
                                color: "#FFFFFF"
                                font.family: "Inter"
                                font.pixelSize: 12
                                clip: true
                                selectByMouse: true
                                Keys.onReturnPressed: {
                                    if (text.length > 0 && typeof bridge !== "undefined") {
                                        bridge.connectWifi(modelData.ssid, text)
                                        root.passwordTargetSsid = ""
                                        text = ""
                                    }
                                }
                                Keys.onEscapePressed: {
                                    root.passwordTargetSsid = ""
                                    text = ""
                                }

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: "Enter Wi-Fi password..."
                                    color: Qt.rgba(1, 1, 1, 0.42)
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                    visible: !passwordInput.text
                                }
                            }

                            Rectangle {
                                Layout.preferredWidth: 62
                                Layout.preferredHeight: 28
                                radius: 6
                                color: (passwordInput.text.length > 0) ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.12)
                                opacity: (passwordInput.text.length > 0) ? 1.0 : 0.5

                                Text {
                                    anchors.centerIn: parent
                                    text: "Connect"
                                    color: "#FFFFFF"
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    enabled: passwordInput.text.length > 0
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (typeof bridge !== "undefined") {
                                            bridge.connectWifi(modelData.ssid, passwordInput.text)
                                            root.passwordTargetSsid = ""
                                            passwordInput.text = ""
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // ── Footer: Network Settings... ───────────────────────────
            Rectangle {
                Layout.fillWidth: true
                height: 30
                radius: 8
                color: netSetMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Image {
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        sourceSize.width: 14
                        sourceSize.height: 14
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/preferences-system-network-symbolic?color=#38BDF8"
                    }

                    Text {
                        text: "Network Settings..."
                        color: "#FFFFFF"
                        font.family: "Inter"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "›"
                        color: Qt.rgba(1, 1, 1, 0.5)
                        font.pixelSize: 14
                    }
                }

                MouseArea {
                    id: netSetMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof bridge !== "undefined") bridge.openWifiSettings()
                    }
                }
            }
        }
    }
}
