// ============================================================================
// NotificationFlyout.qml — Real org.freedesktop.Notifications Center Stack
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    readonly property int shadowPad: 18
    readonly property int cardWidth: 360
    readonly property int notifCount: (typeof bridge !== "undefined" && bridge.notifications)
                                      ? bridge.notifications.length : 0
    readonly property int cardHeight: notifCount > 0 ? Math.min(358, 76 + (Math.min(4, notifCount) * 70)) : 178

    width: cardWidth + shadowPad * 2
    height: cardHeight + shadowPad * 2

    Item {
        id: glassCard
        x: root.shadowPad
        y: root.shadowPad
        width: root.cardWidth
        height: root.cardHeight

        LiquidGlass {
            id: flyoutBg
            anchors.fill: parent
            materialType: "popover"
            cornerRadius: 14
            blurAmount: 28
            fluidOpacity: 0.68
            shadowEnabled: true
            topSpecularOpacity: 0.14
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            // ── Header: Title + Badge Pill + Clear All ───────────────
            Item {
                id: headerRow
                width: parent.width
                height: 24

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Text {
                        text: "Notifications"
                        color: "#FFFFFF"
                        font.family: "Inter"
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    // Count Badge Pill
                    Rectangle {
                        readonly property int count: root.notifCount
                        visible: count > 0
                        width: badgeText.implicitWidth + 12
                        height: 18
                        radius: 9
                        color: Qt.rgba(56 / 255.0, 189 / 255.0, 248 / 255.0, 0.20)
                        border.color: Qt.rgba(56 / 255.0, 189 / 255.0, 248 / 255.0, 0.40)
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            id: badgeText
                            anchors.centerIn: parent
                            text: parent.count + " New"
                            color: "#38BDF8"
                            font.family: "Inter"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }

                // "Clear All" Button
                Rectangle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: clearText.implicitWidth + 14
                    height: 24
                    radius: 6
                    color: clearMouse.pressed ? Qt.rgba(1, 1, 1, 0.22) : (clearMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.06))
                    visible: root.notifCount > 0

                    Text {
                        id: clearText
                        anchors.centerIn: parent
                        text: "Clear All"
                        color: clearMouse.containsMouse ? "#38BDF8" : Qt.rgba(1, 1, 1, 0.78)
                        font.family: "Inter"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }

                    MouseArea {
                        id: clearMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") {
                                bridge.clearNotifications();
                            }
                        }
                    }
                }
            }

            // ── Header Divider Line ──────────────────────────────────
            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(1, 1, 1, 0.09)
            }

            // ── Scrollable Notification Cards List ───────────────────
            ListView {
                id: cardsList
                width: parent.width
                height: Math.min(268, root.notifCount * 70)
                spacing: 6
                clip: true
                visible: root.notifCount > 0
                boundsBehavior: Flickable.StopAtBounds
                model: typeof bridge !== "undefined" ? bridge.notifications : []

                delegate: Rectangle {
                    id: cardItem
                    width: cardsList.width
                    height: 64
                    radius: 10
                    color: (cardMouse.containsMouse || dismissMouse.containsMouse)
                           ? Qt.rgba(1, 1, 1, 0.12)
                           : Qt.rgba(1, 1, 1, 0.05)
                    border.color: Qt.rgba(1, 1, 1, 0.14)
                    border.width: (cardMouse.containsMouse || dismissMouse.containsMouse) ? 1 : 0

                    Behavior on color {
                        ColorAnimation { duration: 110 }
                    }

                    // IMPORTANT: Declare cardMouse BEFORE Row so child dismissMouse receives clicks cleanly!
                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10
                        z: 2

                        // App Icon Badge (36x36)
                        Rectangle {
                            width: 36
                            height: 36
                            radius: 9
                            anchors.verticalCenter: parent.verticalCenter
                            color: Qt.rgba(0, 0, 0, 0.30)
                            border.color: Qt.rgba(1, 1, 1, 0.10)

                            Image {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                source: (modelData.icon && modelData.icon.length > 0)
                                        ? ("image://icon/" + modelData.icon + "?color=#38BDF8")
                                        : "image://icon/dialog-information-symbolic?color=#38BDF8"
                                fillMode: Image.PreserveAspectFit
                            }
                        }

                        // Content (Title, Timestamp, Message Body)
                        Column {
                            width: parent.width - 36 - 24 - 20
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 3

                            Row {
                                width: parent.width
                                spacing: 6

                                Text {
                                    text: modelData.title || "Notification"
                                    color: "#FFFFFF"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    width: Math.min(implicitWidth, parent.width - 66)
                                }

                                Text {
                                    text: "• " + (modelData.time || "Just now")
                                    color: Qt.rgba(1, 1, 1, 0.42)
                                    font.family: "Inter"
                                    font.pixelSize: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            Text {
                                text: modelData.message || ""
                                color: Qt.rgba(1, 1, 1, 0.75)
                                font.family: "Inter"
                                font.pixelSize: 11
                                font.weight: Font.Normal
                                elide: Text.ElideRight
                                width: parent.width
                            }
                        }

                        // Dismiss '×' Action Button
                        Rectangle {
                            width: 24
                            height: 24
                            radius: 12
                            anchors.verticalCenter: parent.verticalCenter
                            color: dismissMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.06)
                            opacity: (cardMouse.containsMouse || dismissMouse.containsMouse) ? 1.0 : 0.48
                            Behavior on opacity { NumberAnimation { duration: 100 } }

                            Image {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                source: "image://icon/window-close-symbolic?color=" +
                                        (dismissMouse.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.78).toString())
                                fillMode: Image.PreserveAspectFit
                            }

                            MouseArea {
                                id: dismissMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof bridge !== "undefined" && typeof bridge.dismissNotification === "function") {
                                        bridge.dismissNotification(index);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Empty State: When no notifications are active ─────────
            Item {
                width: parent.width
                height: 100
                visible: root.notifCount === 0

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    Image {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 28
                        height: 28
                        source: "image://icon/preferences-system-notifications-symbolic?color=" + Qt.rgba(1, 1, 1, 0.30).toString()
                        fillMode: Image.PreserveAspectFit
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "No New Notifications"
                        color: Qt.rgba(1, 1, 1, 0.55)
                        font.family: "Inter"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "You're all caught up."
                        color: Qt.rgba(1, 1, 1, 0.32)
                        font.family: "Inter"
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
