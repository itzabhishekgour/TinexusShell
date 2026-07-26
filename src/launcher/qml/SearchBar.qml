import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: searchBarRoot
    color: "#1E1E2A"
    radius: 8
    border.color: searchInput.activeFocus ? "#6B8CEF80" : "#FFFFFF0F"
    border.width: 1

    Row {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        spacing: 12

        Text {
            text: "🔍"
            font.pixelSize: 18
            anchors.verticalCenter: parent.verticalCenter
        }

        TextInput {
            id: searchInput
            width: parent.width - 40
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 16
            color: "#F0F0F8"
            focus: true
            selectByMouse: true

            Text {
                text: "Search applications, actions, files, or type math (2+2)..."
                color: "#9090A8"
                font.pixelSize: 15
                visible: !parent.text && !parent.inputMethodComposing
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
