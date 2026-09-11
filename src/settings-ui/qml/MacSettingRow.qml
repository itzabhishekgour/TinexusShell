import QtQuick
import QtQuick.Layouts

Item {
    id: root
    Layout.fillWidth: true
    implicitHeight: subtitle.length > 0 ? 48 : 42
    height: implicitHeight

    property string title: ""
    property string subtitle: ""
    property string iconText: ""
    property string iconId: ""
    property color iconColor: "#0A84FF"
    property bool showIcon: iconText.length > 0 || iconId.length > 0
    property bool showSeparator: true
    property int indentSeparator: 16

    default property alias content: controlSlot.data
    property alias controlItem: controlSlot

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        // Optional Icon
        MacSquircleIcon {
            visible: root.showIcon
            iconSize: 20
            glyphSize: 11
            iconColor: root.iconColor
            iconId: root.iconId
            iconText: root.iconText
        }

        // Title and Subtitle (macOS font weight: lighter Normal/Medium, sub-text 11px gray)
        ColumnLayout {
            spacing: 1
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            Text {
                text: root.title
                color: "#f5f5f7"
                font.pixelSize: 13
                font.weight: Font.Normal
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: "#86868b"
                font.pixelSize: 11
                font.weight: Font.Normal
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        // Right control slot
        Item {
            id: controlSlot
            implicitWidth: childrenRect.width
            implicitHeight: childrenRect.height
            Layout.alignment: Qt.AlignVCenter
        }
    }

    // Hairline Separator Line between rows
    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: root.showIcon ? 48 : root.indentSeparator
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#12ffffff"
        visible: root.showSeparator
    }
}
