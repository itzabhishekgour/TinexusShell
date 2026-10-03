// ============================================================================
// PowerConfirmationDialog.qml — System Restart & Shutdown Confirmation Modal
// Ref: Design Doc §14.2
// ============================================================================
import QtQuick
import QtQuick.Layouts
import "../../common/qml"

Item {
    id: root
    width: 380
    height: 180

    property bool isReboot: typeof bridge !== "undefined" && bridge.rebootConfirmationOpen
    property int countdown: 60

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        running: root.visible
        onTriggered: {
            if (root.countdown > 1) {
                root.countdown -= 1;
            } else {
                countdownTimer.stop();
                if (root.isReboot) {
                    bridge.powerReboot();
                } else {
                    bridge.powerShutdown();
                }
            }
        }
    }

    onVisibleChanged: {
        if (visible) {
            root.countdown = 60;
            countdownTimer.restart();
        } else {
            countdownTimer.stop();
        }
    }

    // Shadow
    Rectangle {
        anchors.fill: parent
        anchors.margins: -4
        radius: 14
        color: Qt.rgba(0, 0, 0, 0.40)
    }

    // Card background
    LiquidGlass {
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 12
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        // Title
        Text {
            text: root.isReboot ? "Restart Computer" : "Shut Down Computer"
            color: "#FFFFFF"
            font.pixelSize: 16
            font.bold: true
            Layout.alignment: Qt.AlignHCenter
        }

        // Message with live countdown
        Text {
            text: root.isReboot
                ? "Are you sure you want to restart now?\nSystem will automatically restart in " + root.countdown + " seconds."
                : "Are you sure you want to shut down now?\nSystem will automatically power off in " + root.countdown + " seconds."
            color: Qt.rgba(1, 1, 1, 0.80)
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            Layout.alignment: Qt.AlignHCenter
        }

        Item { Layout.fillHeight: true }

        // Action Buttons
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 16

            // Cancel Button
            Rectangle {
                width: 110
                height: 32
                radius: 8
                color: cancelMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(1, 1, 1, 0.10)
                border.color: Qt.rgba(1, 1, 1, 0.15)
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Cancel"
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    font.weight: Font.Medium
                }

                MouseArea {
                    id: cancelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof bridge !== "undefined") {
                            bridge.closeAllFlyouts();
                        }
                    }
                }
            }

            // Confirm Button
            Rectangle {
                width: 110
                height: 32
                radius: 8
                color: confirmMouse.containsMouse ? (root.isReboot ? "#2563EB" : "#DC2626")
                                                  : (root.isReboot ? "#3B82F6" : "#EF4444")

                Text {
                    anchors.centerIn: parent
                    text: root.isReboot ? "Restart" : "Shut Down"
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    font.bold: true
                }

                MouseArea {
                    id: confirmMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof bridge !== "undefined") {
                            if (root.isReboot) {
                                bridge.powerReboot();
                            } else {
                                bridge.powerShutdown();
                            }
                        }
                    }
                }
            }
        }
    }
}
