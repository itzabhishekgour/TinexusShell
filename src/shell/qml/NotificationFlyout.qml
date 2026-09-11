// ============================================================================
// NotificationFlyout.qml — Notification Center Stack with LiquidGlass
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: 340
    height: 280

    LiquidGlass {
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 12
    }

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // Header
        Item {
            width: parent.width
            height: 20

            Text {
                anchors.left: parent.left
                text: "Notifications"
                color: "#FFFFFF"
                font.pixelSize: 14
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                anchors.right: parent.right
                text: "Clear All"
                color: clearMouse.containsMouse ? "#82A1FF" : Qt.rgba(1, 1, 1, 0.50)
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter

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

        // Notification Cards List
        Column {
            width: parent.width
            spacing: 8

            Repeater {
                model: typeof bridge !== "undefined" ? bridge.notifications : []

                delegate: Rectangle {
                    width: parent.width
                    height: 60
                    radius: 8
                    color: Qt.rgba(1, 1, 1, 0.07)
                    border.color: Qt.rgba(1, 1, 1, 0.08)

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 3

                        Row {
                            spacing: 8
                            Text {
                                text: modelData.title
                                color: "#FFFFFF"
                                font.pixelSize: 13
                                font.bold: true
                            }
                            Text {
                                text: modelData.time
                                color: Qt.rgba(1, 1, 1, 0.40)
                                font.pixelSize: 11
                            }
                        }

                        Text {
                            text: modelData.message
                            color: Qt.rgba(1, 1, 1, 0.70)
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}
