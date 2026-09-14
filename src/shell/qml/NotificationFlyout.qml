// ============================================================================
// NotificationFlyout.qml — Notification Center Stack with LiquidGlass
// Refactored: Modern card delegates, SVG app icons, Inter typography,
//             individual dismiss actions, and empty-state handling
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: 360
    height: Math.max(160, Math.min(420, headerRow.height + 24 + (cardsColumn.implicitHeight > 0 ? cardsColumn.implicitHeight : 100)))

    LiquidGlass {
        id: flyoutBg
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 16
        fluidOpacity: 0.88
    }

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // ── Header: Title + Badge Pill + Clear All ───────────────────
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
                    readonly property int count: typeof bridge !== "undefined" && bridge.notifications
                                                 ? bridge.notifications.length : 0
                    visible: count > 0
                    width: badgeText.implicitWidth + 10
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
                width: clearText.implicitWidth + 12
                height: 22
                radius: 6
                color: clearMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                visible: typeof bridge !== "undefined" && bridge.notifications && bridge.notifications.length > 0

                Text {
                    id: clearText
                    anchors.centerIn: parent
                    text: "Clear All"
                    color: clearMouse.containsMouse ? "#38BDF8" : Qt.rgba(1, 1, 1, 0.60)
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

        // ── Notification Cards Column ────────────────────────────────
        Column {
            id: cardsColumn
            width: parent.width
            spacing: 8
            visible: typeof bridge !== "undefined" && bridge.notifications && bridge.notifications.length > 0

            Repeater {
                model: typeof bridge !== "undefined" ? bridge.notifications : []

                delegate: Rectangle {
                    id: cardItem
                    width: cardsColumn.width
                    height: 64
                    radius: 12
                    color: cardMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(1, 1, 1, 0.05)
                    border.color: cardMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Behavior on color {
                        ColorAnimation { duration: 120 }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

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
                            width: parent.width - 56 - (cardMouse.containsMouse ? 24 : 0)
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 3

                            Row {
                                width: parent.width
                                spacing: 6

                                Text {
                                    text: modelData.title || "System"
                                    color: "#FFFFFF"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    width: Math.min(implicitWidth, parent.width - 60)
                                }

                                Text {
                                    text: "• " + (modelData.time || "Just now")
                                    color: Qt.rgba(1, 1, 1, 0.40)
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

                        // Dismiss 'x' action button (visible on card hover)
                        Rectangle {
                            width: 20
                            height: 20
                            radius: 10
                            anchors.verticalCenter: parent.verticalCenter
                            color: dismissMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.20) : "transparent"
                            visible: cardMouse.containsMouse

                            Image {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                source: "image://icon/window-close-symbolic?color=" +
                                        (dismissMouse.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.60).toString())
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

                    MouseArea {
                        id: cardMouse
                        anchors.fill: parent
                        hoverEnabled: true
                    }
                }
            }
        }

        // ── Empty State: When no notifications are active ───────────
        Item {
            width: parent.width
            height: 100
            visible: typeof bridge === "undefined" || !bridge.notifications || bridge.notifications.length === 0

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
                    color: Qt.rgba(1, 1, 1, 0.50)
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "You're all caught up."
                    color: Qt.rgba(1, 1, 1, 0.30)
                    font.family: "Inter"
                    font.pixelSize: 11
                }
            }
        }
    }
}
