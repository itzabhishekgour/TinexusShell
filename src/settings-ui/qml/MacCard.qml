import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    Layout.fillWidth: true
    radius: 12
    color: Qt.rgba(0.16, 0.17, 0.21, 0.78)
    border.color: Qt.rgba(1, 1, 1, 0.11)
    border.width: 1

    default property alias content: innerContainer.data
    property alias contentItem: innerContainer
    property alias margins: innerContainer.anchors.margins
    property alias topMargin: innerContainer.anchors.topMargin
    property alias bottomMargin: innerContainer.anchors.bottomMargin
    property alias leftMargin: innerContainer.anchors.leftMargin
    property alias rightMargin: innerContainer.anchors.rightMargin

    implicitHeight: innerContainer.implicitHeight + innerContainer.anchors.topMargin + innerContainer.anchors.bottomMargin

    // Subtle 1px top inner specular highlight
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        height: 1
        color: Qt.rgba(1, 1, 1, 0.08)
    }

    Item {
        id: innerContainer
        anchors.fill: parent
        anchors.margins: 0
    }
}
