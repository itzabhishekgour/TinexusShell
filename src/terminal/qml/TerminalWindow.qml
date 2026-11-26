import QtQuick
import QtQuick.Controls
import QtQuick.Window
import tinexus.terminal 1.0
import "../../common/qml"

Window {
    id: root
    width: 860
    height: 540
    minimumWidth: 400
    minimumHeight: 260
    visible: true
    title: "Tinexus Terminal"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    TerminalBridge {
        id: termBridge
    }

    Rectangle {
        id: windowFrame
        anchors.fill: parent
        radius: 12
        color: "#18181c"
        border.color: Qt.rgba(1, 1, 1, 0.10)
        border.width: 1
        clip: true

        // Top CSD Header Bar
        Rectangle {
            id: headerBar
            width: parent.width
            height: 44
            color: "#1f1f26"
            anchors.top: parent.top

            // Window Dragging
            DragHandler {
                target: null
                onActiveChanged: if (active) root.startSystemMove()
            }

            // macOS Style Traffic Lights (shared component with hover glyphs)
            MacTrafficLights {
                targetWindow: root
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                z: 5
            }

            // Window Title
            Text {
                text: "Tinexus Terminal"
                color: "#d1d5db"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                font.family: "Inter"
                anchors.centerIn: parent
            }

            // Monospace Shell Badge
            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: shellTagText.implicitWidth + 14
                height: 20
                radius: 6
                color: Qt.rgba(1, 1, 1, 0.06)
                border.color: Qt.rgba(1, 1, 1, 0.10)
                border.width: 1

                Text {
                    id: shellTagText
                    anchors.centerIn: parent
                    text: "TTY"
                    color: "#9ca3af"
                    font.family: "JetBrains Mono"
                    font.pixelSize: 10
                    font.weight: Font.Medium
                }
            }

            // Hairline Bottom Divider
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Qt.rgba(1, 1, 1, 0.08)
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
