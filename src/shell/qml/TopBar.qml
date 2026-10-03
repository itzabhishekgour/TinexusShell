// ============================================================================
// TopBar.qml — 32px Top Navigation Bar with LiquidGlass Toolbar Material
// Refactored: Pure SVG icon pipeline via TinexusIconProvider (Zero Canvas 2D)
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: parent ? parent.width : (Screen.width > 0 ? Screen.width : 1920)
    height: 32
    readonly property bool notchExpanded: centerNotch.expanded

    // Canonical trigger item references for precise dynamic flyout alignment
    readonly property Item logoTriggerItem: logoBtn
    readonly property Item appTriggerItem: appTrigger
    readonly property Item briTriggerItem: briTrigger
    readonly property Item volTriggerItem: volTrigger
    readonly property Item batTriggerItem: batTrigger
    readonly property Item wifiTriggerItem: wifiTrigger
    readonly property Item notifTriggerItem: notifTrigger
    readonly property Item centerNotchItem: centerNotch

    LiquidGlass {
        id: barBg
        anchors.fill: parent
        materialType: "toolbar"
        cornerRadius: 0
        fluidOpacity: 0.68
    }

    // ── Left: Logo + Applications Dropdown ──────────────────────
    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        // Tinexus Official Logo Button
        Rectangle {
            id: logoBtn
            width: 28
            height: 24
            radius: 5
            color: logoMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            Image {
                id: logoImg
                anchors.centerIn: parent
                width: 18
                height: 18
                source: (typeof bridge !== "undefined" && bridge.logoUrl.length > 0)
                        ? bridge.logoUrl
                        : "image://icon/tinexus-logo"
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: logoMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof bridge !== "undefined") {
                        bridge.toggleLogoMenu();
                    }
                }
            }
        }

        // Clickable "Applications" Dropdown Trigger
        Rectangle {
            id: appTrigger
            height: 24
            width: appRow.implicitWidth + 14
            radius: 5
            color: (appMouse.containsMouse || (typeof bridge !== "undefined" && bridge.appMenuOpen))
                   ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            Row {
                id: appRow
                anchors.centerIn: parent
                spacing: 6

                Image {
                    width: 14
                    height: 14
                    source: "image://icon/view-app-grid-symbolic?color=" +
                            (appMouse.containsMouse || (typeof bridge !== "undefined" && bridge.appMenuOpen) ? "#38BDF8" : "#FFFFFF")
                    fillMode: Image.PreserveAspectFit
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "Applications"
                    color: "#FFFFFF"
                    font.family: "Inter"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }

                Image {
                    width: 9
                    height: 9
                    source: "image://icon/pan-down-symbolic?color=" +
                            (appMouse.containsMouse || (typeof bridge !== "undefined" && bridge.appMenuOpen) ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.60).toString())
                    fillMode: Image.PreserveAspectFit
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: appMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof bridge !== "undefined") {
                        bridge.toggleAppMenu();
                    }
                }
            }
        }
    }

    // ── Center: Reactive Dynamic Island Aura Notch ───────────────
    AuraNotch {
        id: centerNotch
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 1
    }

    // ── Right: Control Center & Status Tray ─────────────────────
    Row {
        id: rightTray
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        // Brightness trigger
        Rectangle {
            id: briTrigger
            width: 28
            height: 24
            radius: 5
            color: briMouse.containsMouse || (typeof bridge !== "undefined" && bridge.brightnessFlyoutOpen)
                   ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "image://icon/display-brightness-symbolic?color=" +
                        (briMouse.containsMouse ? "#38BDF8" : "#F5F5FA")
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: briMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (typeof bridge !== "undefined") bridge.toggleBrightness(); }
            }
        }

        // Volume trigger
        Rectangle {
            id: volTrigger
            width: 28
            height: 24
            radius: 5
            color: volMouse.containsMouse || (typeof bridge !== "undefined" && bridge.volumeFlyoutOpen)
                   ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            readonly property bool isMuted: typeof bridge !== "undefined" && (bridge.soundMuted || bridge.volume === 0)
            readonly property int volLevel: typeof bridge !== "undefined" ? bridge.volume : 50
            readonly property string volIcon: {
                if (isMuted) return "audio-volume-muted-symbolic";
                if (volLevel < 33) return "audio-volume-low-symbolic";
                if (volLevel < 67) return "audio-volume-medium-symbolic";
                return "audio-volume-high-symbolic";
            }

            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "image://icon/" + volTrigger.volIcon + "?color=" +
                        (volMouse.containsMouse ? "#38BDF8" : (volTrigger.isMuted ? "#FF453A" : "#F5F5FA"))
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: volMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (typeof bridge !== "undefined") bridge.toggleVolume(); }
            }
        }

        // Battery status
        Rectangle {
            id: batTrigger
            height: 24
            width: batRow.implicitWidth + 8
            radius: 5
            color: "transparent"
            anchors.verticalCenter: parent.verticalCenter

            readonly property int pct: typeof bridge !== "undefined" ? bridge.batteryPercent : -1
            readonly property string batIcon: {
                if (pct < 0) return "battery-level-100-charged-symbolic";
                if (pct <= 10) return "battery-level-10-symbolic";
                if (pct <= 25) return "battery-level-20-symbolic";
                if (pct <= 45) return "battery-level-40-symbolic";
                if (pct <= 65) return "battery-level-60-symbolic";
                if (pct <= 85) return "battery-level-80-symbolic";
                return "battery-level-100-symbolic";
            }

            Row {
                id: batRow
                anchors.centerIn: parent
                spacing: 5

                Text {
                    text: batTrigger.pct >= 0 ? (batTrigger.pct + "%") : "AC"
                    color: Qt.rgba(1, 1, 1, 0.88)
                    font.family: "Inter"
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    anchors.verticalCenter: parent.verticalCenter
                }

                Image {
                    width: 18
                    height: 14
                    source: "image://icon/" + batTrigger.batIcon + "?color=" +
                            (batTrigger.pct >= 0 && batTrigger.pct <= 20 ? "#FF453A" : "#F5F5FA")
                    fillMode: Image.PreserveAspectFit
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Wi-Fi trigger
        Rectangle {
            id: wifiTrigger
            width: 28
            height: 24
            radius: 5
            color: wifiMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            readonly property bool connected: typeof bridge !== "undefined" ? bridge.networkConnected : false
            readonly property int bars: typeof bridge !== "undefined" ? bridge.networkBars : 0
            readonly property string wifiIcon: {
                if (!connected) return "network-wireless-offline-symbolic";
                if (bars >= 3) return "network-wireless-signal-excellent-symbolic";
                if (bars === 2) return "network-wireless-signal-good-symbolic";
                if (bars === 1) return "network-wireless-signal-weak-symbolic";
                return "network-wireless-signal-none-symbolic";
            }

            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "image://icon/" + wifiTrigger.wifiIcon + "?color=" +
                        (wifiMouse.containsMouse ? "#38BDF8" : (wifiTrigger.connected ? "#F5F5FA" : Qt.rgba(1, 1, 1, 0.45).toString()))
                fillMode: Image.PreserveAspectFit
            }

            MouseArea {
                id: wifiMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof bridge !== "undefined") {
                        bridge.openWifiSettings();
                    }
                }
            }
        }

        // Notification Bell
        Rectangle {
            id: notifTrigger
            width: 28
            height: 24
            radius: 5
            color: notifMouse.containsMouse || (typeof bridge !== "undefined" && bridge.notificationsOpen)
                   ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
            anchors.verticalCenter: parent.verticalCenter

            Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: "image://icon/preferences-system-notifications-symbolic?color=" +
                        (notifMouse.containsMouse ? "#38BDF8" : "#F5F5FA")
                fillMode: Image.PreserveAspectFit
            }

            // Unread orange badge dot
            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: 3
                anchors.right: parent.right
                anchors.rightMargin: 4
                width: 6
                height: 6
                radius: 3
                color: "#FF9500"
                visible: typeof bridge !== "undefined" && bridge.notifications && bridge.notifications.length > 0
            }

            MouseArea {
                id: notifMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (typeof bridge !== "undefined") bridge.toggleNotifications(); }
            }
        }
    }
}
