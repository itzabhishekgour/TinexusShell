import QtQuick

Item {
    id: root
    width: 40
    height: 22

    property bool checked: false
    property bool enabled: true
    signal toggled(bool checked)

    opacity: enabled ? 1.0 : 0.45

    Rectangle {
        id: track
        anchors.fill: parent
        radius: 11
        color: root.checked ? (typeof bridge !== "undefined" ? bridge.accentColor : "#30d158") : "#39393d"
        border.color: root.checked ? Qt.rgba(1, 1, 1, 0.22) : "#505056"
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 140; easing.type: Easing.OutCubic }
        }

        // Thumb contact shadow
        Rectangle {
            x: thumb.x
            y: thumb.y + 1
            width: thumb.width
            height: thumb.height
            radius: thumb.radius
            color: Qt.rgba(0, 0, 0, 0.28)
        }

        Rectangle {
            id: thumb
            width: mouseArea.pressed ? 20 : 18
            height: 18
            radius: 9
            y: 2
            x: root.checked ? (root.width - width - 2) : 2
            color: "#ffffff"
            border.color: Qt.rgba(0, 0, 0, 0.12)
            border.width: 1

            Behavior on width {
                NumberAnimation { duration: 90; easing.type: Easing.OutQuad }
            }
            Behavior on x {
                SpringAnimation {
                    spring: 5.8
                    damping: 0.72
                    epsilon: 0.05
                }
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
