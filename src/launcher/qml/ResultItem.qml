import QtQuick 2.15

Rectangle {
    id: resultItemRoot
    color: ListView.isCurrentItem ? "#6B8CEF1A" : "transparent"
    radius: 6
    border.color: ListView.isCurrentItem ? "#6B8CEF80" : "transparent"
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        Rectangle {
            width: 32
            height: 32
            radius: 6
            color: "#1E1E2A"
            anchors.verticalCenter: parent.verticalCenter

            Text {
                text: "⚡"
                font.pixelSize: 16
                anchors.centerIn: parent
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - 50

            Text {
                text: model.title || "Firefox Web Browser"
                color: "#F0F0F8"
                font.pixelSize: 14
                font.bold: true
            }

            Text {
                text: model.subtitle || "Web Browser"
                color: "#9090A8"
                font.pixelSize: 12
            }
        }
    }
}
