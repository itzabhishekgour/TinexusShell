// ============================================================================
// ApplicationsFlyout.qml — Dropdown List of Installed Applications
// Refined to match Apple LogoMenuFlyout aesthetic
// ============================================================================
import QtQuick
import QtQuick.Controls
import "../../common/qml"

Item {
    id: root
    width: 220
    height: Math.min(360, (appList.count * 28) + 16)

    LiquidGlass {
        id: bg
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 10
    }

    ListView {
        id: appList
        anchors.fill: parent
        anchors.margins: 6
        clip: true
        spacing: 2
        model: typeof bridge !== "undefined" ? bridge.applicationsList : []

        ScrollBar.vertical: ScrollBar {
            policy: appList.contentHeight > appList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
        }

        delegate: Rectangle {
            id: itemRect
            width: appList.width
            height: 26
            radius: 5
            color: itemMouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.30) : "transparent"

            Row {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter

                // Minimalist Accent Dot
                Rectangle {
                    width: 5
                    height: 5
                    radius: 2.5
                    color: itemMouse.containsMouse ? "#38BDF8" : Qt.rgba(1, 1, 1, 0.40)
                    anchors.verticalCenter: parent.verticalCenter
                }

                // App Title
                Text {
                    text: modelData.name
                    color: "#FFFFFF"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: itemMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof bridge !== "undefined") {
                        bridge.launchApp(modelData.exec);
                        bridge.closeAllFlyouts();
                    }
                }
            }
        }
    }
}

