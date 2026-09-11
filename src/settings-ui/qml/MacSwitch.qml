import QtQuick

Item {
    id: root
    width: 38
    height: 22

    property bool checked: false
    signal toggled(bool checked)

    Rectangle {
        id: track
        anchors.fill: parent
        radius: 11
        // Flat matte Apple green for dark mode (no glow, no oversaturation)
        color: root.checked ? "#30d158" : "#39393d"
        border.color: root.checked ? "#24a143" : "#48484c"
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 150; easing.type: Easing.OutQuad }
        }

        // Thumb subtle drop shadow (matte macOS style)
        Rectangle {
            x: thumb.x
            y: thumb.y + 1
            width: thumb.width
            height: thumb.height
            radius: thumb.radius
            color: Qt.rgba(0, 0, 0, 0.25)
            visible: true
        }

        // Thumb
        Rectangle {
            id: thumb
            width: 18
            height: 18
            radius: 9
            y: 2
            x: root.checked ? 18 : 2
            color: "#ffffff"

            border.color: Qt.rgba(0, 0, 0, 0.10)
            border.width: 1

            Behavior on x {
                NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            root.checked = !root.checked
            root.toggled(root.checked)
        }
    }
}
