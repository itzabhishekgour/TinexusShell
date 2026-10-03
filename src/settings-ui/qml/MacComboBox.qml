import QtQuick
import QtQuick.Controls

ComboBox {
    id: root
    implicitWidth: Math.max(140, contentItem.implicitWidth + 36)
    implicitHeight: 28

    delegate: ItemDelegate {
        width: root.width
        height: 28
        highlighted: root.highlightedIndex === index

        contentItem: Row {
            spacing: 8
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10

            Text {
                text: root.currentIndex === index ? "✓" : " "
                color: "#ffffff"
                font.pixelSize: 12
                font.bold: true
                width: 14
            }

            Text {
                text: modelData
                color: highlighted ? "#ffffff" : "#f5f5f7"
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            color: highlighted ? "#0A84FF" : (hovered ? "#14ffffff" : "transparent")
            radius: 4
            anchors.fill: parent
            anchors.margins: 2
        }
    }

    indicator: Text {
        x: root.width - width - 10
        y: root.topPadding + (root.availableHeight - height) / 2
        text: "▾"
        font.pixelSize: 12
        color: "#98989d"
    }

    contentItem: Text {
        leftPadding: 10
        rightPadding: root.indicator.width + 12
        text: root.displayText
        font.pixelSize: 12
        font.weight: Font.Medium
        color: "#f5f5f7"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 28
        color: root.down ? "#28282a" : (root.hovered ? "#3e3e42" : "#323236")
        border.color: "#1fffffff"
        border.width: 1
        radius: 6

        Behavior on color {
            ColorAnimation { duration: 80 }
        }
    }

    popup: Popup {
        y: root.height + 4
        width: root.width
        implicitHeight: contentItem.implicitHeight + 8
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: root.popup.visible ? root.delegateModel : null
            currentIndex: root.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: "#28282a"
            border.color: "#26ffffff"
            border.width: 1
            radius: 8
        }
    }
}
