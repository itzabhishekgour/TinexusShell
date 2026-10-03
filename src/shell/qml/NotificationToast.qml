// ============================================================================
// NotificationToast.qml — Floating Toast Alert for tinexus-shell
// Ref: docs/05_UI_UX_GUIDELINES.md §6.1, §6.4
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: 350
    height: 68
    visible: typeof bridge !== "undefined" && bridge.toastVisible
    opacity: visible ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }

    LiquidGlass {
        id: toastBg
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 14
        fluidOpacity: 0.92
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (typeof bridge !== "undefined") {
                bridge.dismissToast();
                bridge.setNotificationsOpen(true);
            }
        }
    }

    Row {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // App Icon Badge (38x38)
        Rectangle {
            width: 38
            height: 38
            radius: 10
            anchors.verticalCenter: parent.verticalCenter
            color: Qt.rgba(0, 0, 0, 0.35)
            border.color: Qt.rgba(1, 1, 1, 0.12)

            Image {
                anchors.centerIn: parent
                width: 22
                height: 22
                source: (typeof bridge !== "undefined" && bridge.toastIcon && bridge.toastIcon.length > 0)
                        ? ("image://icon/" + bridge.toastIcon + "?color=#38BDF8")
                        : "image://icon/dialog-information-symbolic?color=#38BDF8"
                fillMode: Image.PreserveAspectFit
            }
        }

        // Text details (Title + Body)
        Column {
            width: parent.width - 38 - 10 - 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: typeof bridge !== "undefined" ? bridge.toastTitle : ""
                color: "#FFFFFF"
                font.family: "Inter"
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                width: parent.width
            }

            Text {
                text: typeof bridge !== "undefined" ? bridge.toastMessage : ""
                color: Qt.rgba(1, 1, 1, 0.82)
                font.family: "Inter"
                font.pixelSize: 11
                font.weight: Font.Normal
                elide: Text.ElideRight
                maximumLineCount: 2
                wrapMode: Text.WrapAnywhere
                width: parent.width
            }
        }

        // Close / Dismiss 'X' Button
        Rectangle {
            width: 22
            height: 22
            radius: 11
            color: closeMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.08)
            anchors.verticalCenter: parent.verticalCenter

            Text {
                anchors.centerIn: parent
                text: "✕"
                color: closeMouse.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.6)
                font.pixelSize: 10
                font.bold: true
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (typeof bridge !== "undefined") {
                        bridge.dismissToast();
                    }
                }
            }
        }
    }
}
