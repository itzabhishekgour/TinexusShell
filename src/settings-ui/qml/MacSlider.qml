import QtQuick

Item {
    id: root
    implicitWidth: 200
    implicitHeight: 24

    property real from: 0
    property real to: 100
    property real stepSize: 1
    property real value: 50
    property bool enabled: true

    signal moved(real val)

    opacity: root.enabled ? 1.0 : 0.45

    function updateValueFromMouse(mouseX) {
        if (!root.enabled) return
        var padding = thumb.width / 2
        var availableWidth = root.width - thumb.width
        if (availableWidth <= 0) return
        var pos = Math.max(0, Math.min(mouseX - padding, availableWidth))
        var ratio = pos / availableWidth
        var rawVal = root.from + ratio * (root.to - root.from)
        if (root.stepSize > 0) {
            rawVal = Math.round((rawVal - root.from) / root.stepSize) * root.stepSize + root.from
        }
        rawVal = Math.max(root.from, Math.min(root.to, rawVal))
        if (root.value !== rawVal) {
            root.value = rawVal
            root.moved(rawVal)
        }
    }

    // Groove
    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.right: parent.right
        height: 6
        radius: 3
        color: "#38383c"

        // Filled progress track
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            radius: 3
            width: {
                var range = root.to - root.from
                if (range <= 0) return 0
                var pct = Math.max(0, Math.min(1.0, (root.value - root.from) / range))
                return pct * parent.width
            }
            color: bridge ? bridge.accentColor : "#007aff"
        }
    }

    // Thumb
    Rectangle {
        id: thumb
        width: 18
        height: 18
        radius: 9
        anchors.verticalCenter: parent.verticalCenter
        x: {
            var range = root.to - root.from
            if (range <= 0) return 0
            var pct = Math.max(0, Math.min(1.0, (root.value - root.from) / range))
            return pct * (root.width - width)
        }
        color: "#ffffff"
        border.color: "#c8c8cc"
        border.width: 1

        Rectangle {
            anchors.fill: parent
            radius: 9
            color: "transparent"
            border.color: Qt.rgba(0, 0, 0, 0.2)
            border.width: 1
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        preventStealing: true

        onPressed: (mouse) => root.updateValueFromMouse(mouse.x)
        onPositionChanged: (mouse) => {
            if (pressed) {
                root.updateValueFromMouse(mouse.x)
            }
        }
    }
}
