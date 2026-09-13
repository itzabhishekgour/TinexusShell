// ============================================================================
// TopBar.qml — 32px Top Navigation Bar with LiquidGlass Toolbar Material
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: parent ? parent.width : 1920
    height: 32

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
                width: 20
                height: 20
                source: typeof bridge !== "undefined" ? bridge.logoUrl : ""
                fillMode: Image.PreserveAspectFit
                visible: status === Image.Ready
            }

            // Fallback circular badge if logo image file isn't loaded yet
            Rectangle {
                anchors.centerIn: parent
                width: 18
                height: 18
                radius: 9
                color: "#1E2330"
                border.color: "#3B82F6"
                border.width: 1.2
                visible: !logoImg.visible

                Text {
                    anchors.centerIn: parent
                    text: "T"
                    color: "#FFFFFF"
                    font.pixelSize: 10
                    font.bold: true
                }
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

                Text {
                    text: "Applications"
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "▾"
                    color: Qt.rgba(1, 1, 1, 0.70)
                    font.pixelSize: 10
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

    // ── Center: Dynamic Sloped Trapezoid Notch ──────────────────
    AuraNotch {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
    }

    // ── Right: Control Center & Status Tray ─────────────────────
    Row {
        id: rightTray
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        // Brightness trigger (Sun vector)
        Item {
            id: briTrigger
            width: 22
            height: 22
            anchors.verticalCenter: parent.verticalCenter

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = "#FFFFFF";
                    ctx.fillStyle = "#FFFFFF";
                    ctx.lineWidth = 1.5;
                    var cx = 11, cy = 11, r = 4;
                    ctx.beginPath();
                    ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                    ctx.stroke();
                    var angles = [0, 45, 90, 135, 180, 225, 270, 315];
                    for (var i = 0; i < angles.length; i++) {
                        var rad = angles[i] * Math.PI / 180.0;
                        var x1 = cx + Math.cos(rad) * 6.5;
                        var y1 = cy + Math.sin(rad) * 6.5;
                        var x2 = cx + Math.cos(rad) * 9.0;
                        var y2 = cy + Math.sin(rad) * 9.0;
                        ctx.beginPath();
                        ctx.moveTo(x1, y1);
                        ctx.lineTo(x2, y2);
                        ctx.stroke();
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (typeof bridge !== "undefined") bridge.toggleBrightness(); }
            }
        }

        // Volume trigger (Speaker vector)
        Item {
            id: volTrigger
            width: 22
            height: 22
            anchors.verticalCenter: parent.verticalCenter

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = "#FFFFFF";
                    ctx.fillStyle = "#FFFFFF";
                    ctx.lineWidth = 1.4;
                    // Speaker body
                    ctx.beginPath();
                    ctx.rect(3, 8, 4, 6);
                    ctx.fill();
                    // Cone
                    ctx.beginPath();
                    ctx.moveTo(7, 8);
                    ctx.lineTo(12, 4);
                    ctx.lineTo(12, 18);
                    ctx.lineTo(7, 14);
                    ctx.closePath();
                    ctx.fill();
                    // Sound waves
                    ctx.beginPath();
                    ctx.arc(12, 11, 4, -Math.PI / 3, Math.PI / 3, false);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.arc(12, 11, 7.5, -Math.PI / 3, Math.PI / 3, false);
                    ctx.stroke();
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (typeof bridge !== "undefined") bridge.toggleVolume(); }
            }
        }

        // Battery status
        Row {
            id: batTrigger
            spacing: 5
            anchors.verticalCenter: parent.verticalCenter
            readonly property int pct: typeof bridge !== "undefined" ? bridge.batteryPercent : -1

            Text {
                text: parent.pct >= 0 ? (parent.pct + "%") : "AC"
                color: Qt.rgba(1, 1, 1, 0.85)
                font.pixelSize: 11
                font.weight: Font.Medium
                anchors.verticalCenter: parent.verticalCenter
            }

            // Battery Outline & Level
            Item {
                width: 24
                height: 12
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: 20
                    height: 12
                    radius: 3
                    color: "transparent"
                    border.color: Qt.rgba(1, 1, 1, 0.70)
                    border.width: 1

                    Rectangle {
                        x: 2; y: 2
                        width: Math.max(2, Math.round(16 * (batTrigger.pct >= 0 ? Math.min(100, batTrigger.pct) : 100) / 100.0))
                        height: 8
                        radius: 1.5
                        color: batTrigger.pct < 0 ? "#38BDF8" : (batTrigger.pct > 20 ? "#34C759" : "#FF453A")
                    }
                }

                // Battery positive terminal nub
                Rectangle {
                    x: 21; y: 4
                    width: 2
                    height: 4
                    radius: 1
                    color: Qt.rgba(1, 1, 1, 0.70)
                }
            }
        }

        // Wi-Fi (Vector arcs — dynamic signal strength)
        Item {
            id: wifiTrigger
            width: 20
            height: 18
            anchors.verticalCenter: parent.verticalCenter

            Canvas {
                id: wifiCanvas
                anchors.fill: parent

                // Read live signal state from ShellBridge
                readonly property bool connected: typeof bridge !== "undefined" ? bridge.networkConnected : false
                readonly property int bars: typeof bridge !== "undefined" ? bridge.networkBars : 0

                // Repaint whenever signal state changes
                onConnectedChanged: requestPaint()
                onBarsChanged: requestPaint()
                Component.onCompleted: requestPaint()

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    var cx = 10, cy = 15;
                    var arcRadii  = [4.5, 8.0, 11.5];  // 3 arcs = 3 bar levels beyond center dot
                    var barThresh = [1, 2, 3];           // bar count required to light each arc

                    var activeColor = Qt.rgba(1, 1, 1, 0.90).toString();
                    var dimColor    = Qt.rgba(1, 1, 1, 0.25).toString();

                    ctx.lineWidth = 1.6;
                    ctx.lineCap = "round";

                    // Center dot (always visible)
                    ctx.fillStyle = connected ? activeColor : dimColor;
                    ctx.beginPath();
                    ctx.arc(cx, cy, 1.6, 0, 2 * Math.PI);
                    ctx.fill();

                    // Arcs: light up based on signal bar count
                    for (var i = 0; i < 3; i++) {
                        ctx.strokeStyle = (connected && bars >= barThresh[i]) ? activeColor : dimColor;
                        ctx.beginPath();
                        ctx.arc(cx, cy, arcRadii[i], -Math.PI * 0.75, -Math.PI * 0.25);
                        ctx.stroke();
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof bridge !== "undefined") {
                        bridge.openWifiSettings();
                    }
                }
            }
        }

        // Notification Bell (Vector bell)
        Item {
            id: notifTrigger
            width: 20
            height: 20
            anchors.verticalCenter: parent.verticalCenter

            Canvas {
                anchors.fill: parent
                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.strokeStyle = "#FFFFFF";
                    ctx.fillStyle = "#FFFFFF";
                    ctx.lineWidth = 1.3;
                    ctx.beginPath();
                    ctx.moveTo(10, 3);
                    ctx.bezierCurveTo(7, 3, 6, 7, 6, 12);
                    ctx.lineTo(4, 14);
                    ctx.lineTo(16, 14);
                    ctx.lineTo(14, 12);
                    ctx.bezierCurveTo(14, 7, 13, 3, 10, 3);
                    ctx.stroke();
                    ctx.beginPath();
                    ctx.arc(10, 15.5, 1.8, 0, Math.PI);
                    ctx.fill();
                    ctx.beginPath();
                    ctx.arc(10, 3, 1.2, 0, 2 * Math.PI);
                    ctx.stroke();
                }
            }

            // Unread orange badge dot
            Rectangle {
                anchors.top: parent.top
                anchors.topMargin: 2
                anchors.right: parent.right
                anchors.rightMargin: 1
                width: 5
                height: 5
                radius: 2.5
                color: "#FF9500"
                visible: typeof bridge !== "undefined" && bridge.notifications && bridge.notifications.length > 0
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (typeof bridge !== "undefined") bridge.toggleNotifications(); }
            }
        }
    }
}
