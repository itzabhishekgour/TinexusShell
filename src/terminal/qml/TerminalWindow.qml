import QtQuick
import QtQuick.Controls
import QtQuick.Window
import tinexus.terminal 1.0

Window {
    id: root
    width: 860
    height: 540
    minimumWidth: 400
    minimumHeight: 260
    visible: true
    title: "Tinexus Terminal"
    color: "transparent"

    TerminalBridge {
        id: termBridge
    }

    Rectangle {
        id: windowFrame
        anchors.fill: parent
        radius: 12
        color: "#18181c"
        border.color: "#2c2c36"
        border.width: 1
        clip: true

        // Top CSD Header Bar
        Rectangle {
            id: headerBar
            width: parent.width
            height: 38
            color: "#1f1f26"
            anchors.top: parent.top

            // macOS Style Traffic Lights
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                // Close (Red)
                Rectangle {
                    width: 12; height: 12; radius: 6
                    color: "#ff5f56"
                    border.color: "#e0443e"
                    border.width: 1
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.close()
                    }
                }

                // Minimize (Yellow)
                Rectangle {
                    width: 12; height: 12; radius: 6
                    color: "#ffbd2e"
                    border.color: "#dea123"
                    border.width: 1
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.showMinimized()
                    }
                }

                // Maximize / Restore (Green)
                Rectangle {
                    width: 12; height: 12; radius: 6
                    color: "#27c93f"
                    border.color: "#1aab29"
                    border.width: 1
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.visibility === Window.Maximized) {
                                root.showNormal();
                            } else {
                                root.showMaximized();
                            }
                        }
                    }
                }
            }

            // Window Title
            Text {
                text: "Tinexus Terminal"
                color: "#9898a6"
                font.pixelSize: 12
                font.bold: true
                font.family: "Inter, sans-serif"
                anchors.centerIn: parent
            }

            // Window Dragging
            DragHandler {
                target: null
                onActiveChanged: if (active) root.startSystemMove()
            }
        }

        // Terminal Grid Canvas
        TerminalCanvas {
            id: canvas
            anchors.top: headerBar.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 4
            bridge: termBridge
            focus: true

            Component.onCompleted: {
                canvas.forceActiveFocus();
            }
        }
    }
}
