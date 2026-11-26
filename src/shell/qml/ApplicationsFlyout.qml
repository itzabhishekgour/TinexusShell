// ============================================================================
// ApplicationsFlyout.qml — Dropdown List of Installed Applications
// Unified LiquidGlass popover styling matching CalendarFlyout & NotificationFlyout
// ============================================================================
import QtQuick
import QtQuick.Controls
import "../../common/qml"

Item {
    id: root
    readonly property int shadowPad: 18
    readonly property int cardWidth: 248
    readonly property int cardHeight: Math.min(372, Math.max(88, 56 + (appList.count * 34)))

    width: cardWidth + shadowPad * 2
    height: cardHeight + shadowPad * 2

    Item {
        id: glassCard
        x: root.shadowPad
        y: root.shadowPad
        width: root.cardWidth
        height: root.cardHeight

        LiquidGlass {
            id: bg
            anchors.fill: parent
            materialType: "popover"
            cornerRadius: 14
            blurAmount: 28
            fluidOpacity: 0.66
            shadowEnabled: true
            topSpecularOpacity: 0.14
        }

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8

            // Header
            Item {
                width: parent.width
                height: 22

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Applications"
                    color: "#FFFFFF"
                    font.family: "Inter"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: countText.implicitWidth + 10
                    height: 18
                    radius: 9
                    color: Qt.rgba(56 / 255.0, 189 / 255.0, 248 / 255.0, 0.18)
                    border.color: Qt.rgba(56 / 255.0, 189 / 255.0, 248 / 255.0, 0.35)
                    visible: appList.count > 0

                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: appList.count.toString()
                        color: "#38BDF8"
                        font.family: "Inter"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(1, 1, 1, 0.09)
            }

            ListView {
                id: appList
                width: parent.width
                height: Math.max(24, parent.height - 39)
                clip: true
                spacing: 2
                model: typeof bridge !== "undefined" ? bridge.applicationsList : []

                ScrollBar.vertical: ScrollBar {
                    policy: appList.contentHeight > appList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                delegate: Rectangle {
                    width: appList.width
                    height: 32
                    radius: 8
                    color: itemMouse.containsMouse
                           ? Qt.rgba(0.36, 0.56, 0.96, 0.32)
                           : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: 90 }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 10

                        Image {
                            width: 18
                            height: 18
                            anchors.verticalCenter: parent.verticalCenter
                            fillMode: Image.PreserveAspectFit
                            sourceSize.width: 36
                            sourceSize.height: 36
                            source: "image://icon/" + ((modelData.icon && modelData.icon.length > 0)
                                    ? modelData.icon
                                    : "application-x-executable")
                        }

                        Text {
                            text: modelData.name
                            color: "#FFFFFF"
                            font.family: "Inter"
                            font.pixelSize: 13
                            font.weight: itemMouse.containsMouse ? Font.Medium : Font.Normal
                            elide: Text.ElideRight
                            width: parent.width - 30
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") {
                                bridge.launchApp(modelData.exec);
                                bridge.closeAllFlyouts();
                            }
                        }
                    }
                }
            }
        }
    }
}
