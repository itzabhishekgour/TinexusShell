// ============================================================================
// LogoMenuFlyout.qml — Apple-style Logo Menu Flyout with LiquidGlass
// ============================================================================
import QtQuick
import QtQuick.Layouts
import "../../common/qml"

Item {
    id: root
    width: 224
    height: 274

    LiquidGlass {
        id: bg
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 12
        fluidOpacity: 0.65
    }

    Column {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 2

        Repeater {
            model: [
                { label: "About Tinexus",      action: "launch", arg: "tinexus-about",       dividerAfter: true },
                { label: "System Settings...", action: "launch", arg: "tinexus-settings-ui", dividerAfter: false },
                { label: "App Store",          action: "launch", arg: "tinexus-store",       dividerAfter: false },
                { label: "Activity Monitor",   action: "launch", arg: "tinexus-monitor",     dividerAfter: true },
                { label: "Lock Screen",        action: "launch", arg: "tinexus-lock",        dividerAfter: false },
                { label: "Sleep",              action: "sleep",  arg: "",                    dividerAfter: true },
                { label: "Restart...",         action: "reboot", arg: "",                    dividerAfter: false },
                { label: "Shut Down...",       action: "power",  arg: "",                    dividerAfter: false }
            ]

            delegate: Column {
                width: root.width - 12
                spacing: 0

                Rectangle {
                    width: parent.width
                    height: 30
                    radius: 8
                    color: rowMouse.containsMouse
                           ? Qt.rgba(0.36, 0.56, 0.96, 0.32)
                           : "transparent"

                    Behavior on color {
                        ColorAnimation { duration: 90 }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        color: "#FFFFFF"
                        font.pixelSize: 13
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge === "undefined") return;
                            if (modelData.action === "launch") {
                                bridge.launchApp(modelData.arg);
                                bridge.closeAllFlyouts();
                            } else if (modelData.action === "sleep") {
                                bridge.powerSleep();
                                bridge.closeAllFlyouts();
                            } else if (modelData.action === "reboot") {
                                bridge.requestReboot();
                            } else if (modelData.action === "power") {
                                bridge.requestShutdown();
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 6
                    visible: modelData.dividerAfter

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        height: 1
                        color: Qt.rgba(1, 1, 1, 0.11)
                    }
                }
            }
        }
    }
}
