import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    Layout.fillWidth: true
    radius: 10
    color: "#28282a" // macOS grouped list background (neutral dark graphite, no purple tint)
    border.color: "#14ffffff"
    border.width: 1

    default property alias content: innerContainer.data
    property alias contentItem: innerContainer
    property alias margins: innerContainer.anchors.margins
    property alias topMargin: innerContainer.anchors.topMargin
    property alias bottomMargin: innerContainer.anchors.bottomMargin
    property alias leftMargin: innerContainer.anchors.leftMargin
    property alias rightMargin: innerContainer.anchors.rightMargin

    implicitHeight: innerContainer.implicitHeight + innerContainer.anchors.topMargin + innerContainer.anchors.bottomMargin

    Item {
        id: innerContainer
        anchors.fill: parent
        anchors.margins: 0
    }
}
