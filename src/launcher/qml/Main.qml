import QtQuick 2.15
import QtQuick.Controls 2.15

Window {
    id: rootWindow
    width: 680
    height: 420
    visible: true
    title: "Tinexus Command Palette"
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    Rectangle {
        id: mainCard
        anchors.fill: parent
        color: "#13131A" // 80% opacity glassmorphism surface
        radius: 12
        border.color: "#FFFFFF0F"
        border.width: 1

        Column {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12

            SearchBar {
                id: searchBar
                width: parent.width
                height: 48
            }

            Rectangle {
                width: parent.width
                height: 1
                color: "#FFFFFF0F"
            }

            ResultList {
                id: resultList
                width: parent.width
                height: parent.height - searchBar.height - 29
            }
        }
    }
}
