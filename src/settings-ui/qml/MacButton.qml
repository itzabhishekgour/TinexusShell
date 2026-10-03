import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property string text: ""
    property bool primary: false
    property bool destructive: false
    property bool enabled: true
    signal clicked()

    implicitWidth: Math.max(72, btnLabel.implicitWidth + 24)
    implicitHeight: 28
    radius: 6

    opacity: root.enabled ? 1.0 : 0.5

    color: {
        if (!root.enabled) return "#2c2c2e"
        if (btnArea.pressed) {
            return root.destructive ? "#b8241c" : (root.primary ? "#0062cc" : "#242426")
        }
        if (btnArea.containsMouse) {
            return root.destructive ? "#e63327" : (root.primary ? "#1a8cff" : "#444448")
        }
        return root.destructive ? "#ff3b30" : (root.primary ? "#0A84FF" : "#343438")
    }

    border.color: {
        if (root.primary) return "#0071e3"
        if (root.destructive) return "#cc291f"
        return "#1fffffff"
    }
    border.width: 1

    Behavior on color {
        ColorAnimation { duration: 100 }
    }

    Text {
        id: btnLabel
        anchors.centerIn: parent
        text: root.text
        color: root.enabled ? "#ffffff" : "#86868b"
        font.pixelSize: 12
        font.weight: Font.Medium
    }

    MouseArea {
        id: btnArea
        anchors.fill: parent
        hoverEnabled: root.enabled
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            if (root.enabled) {
                root.clicked()
            }
        }
    }
}
