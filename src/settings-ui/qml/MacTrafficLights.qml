import QtQuick
import QtQuick.Controls

Row {
    id: root
    spacing: 8

    property var targetWindow: (typeof window !== "undefined" ? window : (typeof rootWindow !== "undefined" ? rootWindow : root.Window.window))
    property bool isHovered: trafficHoverArea.containsMouse

    MouseArea {
        id: trafficHoverArea
        width: 56
        height: 16
        hoverEnabled: true
        acceptedButtons: Qt.NoButton

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            // ── Close (Red) ────────────────────────────────────────────────
            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: closeBtnArea.pressed ? "#bf453f" : (root.isHovered ? "#ff7b73" : "#ff5f56")
                border.color: "#e0443e"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    color: "#4d0000"
                    font.pixelSize: 11
                    font.bold: true
                    visible: root.isHovered
                }

                MouseArea {
                    id: closeBtnArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var win = root.targetWindow ? root.targetWindow : root.Window.window
                        if (win) {
                            if (typeof win.close === "function") {
                                win.close()
                            } else {
                                win.visible = false
                            }
                        } else {
                            Qt.quit()
                        }
                    }
                }
            }

            // ── Minimize (Yellow) ──────────────────────────────────────────
            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: minBtnArea.pressed ? "#c99524" : (root.isHovered ? "#ffd359" : "#ffbd2e")
                border.color: "#dea123"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "–"
                    color: "#5c3d00"
                    font.pixelSize: 11
                    font.bold: true
                    visible: root.isHovered
                }

                MouseArea {
                    id: minBtnArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var win = root.targetWindow ? root.targetWindow : root.Window.window
                        if (win) {
                            if (typeof win.showMinimized === "function") {
                                win.showMinimized()
                            } else {
                                win.visibility = Window.Minimized
                            }
                        }
                    }
                }
            }

            // ── Maximize / Zoom (Green) ────────────────────────────────────
            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: maxBtnArea.pressed ? "#1e9e31" : (root.isHovered ? "#46e05c" : "#27c93f")
                border.color: "#1aab29"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "+"
                    color: "#004000"
                    font.pixelSize: 10
                    font.bold: true
                    visible: root.isHovered
                }

                MouseArea {
                    id: maxBtnArea
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var win = root.targetWindow ? root.targetWindow : root.Window.window
                        if (win) {
                            if (win.visibility === Window.Maximized) {
                                win.showNormal()
                            } else {
                                win.showMaximized()
                            }
                        }
                    }
                }
            }
        }
    }
}
