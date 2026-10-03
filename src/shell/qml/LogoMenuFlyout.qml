// ============================================================================
// LogoMenuFlyout.qml — Apple-style Logo Menu Flyout with LiquidGlass
// ============================================================================
import QtQuick
import QtQuick.Layouts
import "../../common/qml"

Item {
    id: root
    width: 220
    height: 290

    LiquidGlass {
        id: bg
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 10
    }

    Column {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 2

        // Item 1: About Tinexus
        Rectangle {
            width: parent.width; height: 26; radius: 5
            color: item1Mouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.3) : "transparent"
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "About Tinexus"; color: "#FFFFFF"; font.pixelSize: 13 }
            MouseArea { id: item1Mouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (typeof bridge !== "undefined") { bridge.launchApp("tinexus-about"); bridge.closeAllFlyouts(); } } }
        }

        // Divider
        Rectangle { width: parent.width - 16; anchors.horizontalCenter: parent.horizontalCenter; height: 1; color: Qt.rgba(1, 1, 1, 0.10) }

        // Item 2: System Settings
        Rectangle {
            width: parent.width; height: 26; radius: 5
            color: item2Mouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.3) : "transparent"
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "System Settings..."; color: "#FFFFFF"; font.pixelSize: 13 }
            MouseArea { id: item2Mouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (typeof bridge !== "undefined") { bridge.launchApp("tinexus-settings-ui"); bridge.closeAllFlyouts(); } } }
        }

        // Item 3: App Store (Restored)
        Rectangle {
            width: parent.width; height: 26; radius: 5
            color: itemStoreMouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.3) : "transparent"
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "App Store"; color: "#FFFFFF"; font.pixelSize: 13 }
            MouseArea { id: itemStoreMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (typeof bridge !== "undefined") { bridge.launchApp("tinexus-store"); bridge.closeAllFlyouts(); } } }
        }

        // Item 4: Activity Monitor
        Rectangle {
            width: parent.width; height: 26; radius: 5
            color: item3Mouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.3) : "transparent"
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Activity Monitor"; color: "#FFFFFF"; font.pixelSize: 13 }
            MouseArea { id: item3Mouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (typeof bridge !== "undefined") { bridge.launchApp("tinexus-monitor"); bridge.closeAllFlyouts(); } } }
        }

        // Divider
        Rectangle { width: parent.width - 16; anchors.horizontalCenter: parent.horizontalCenter; height: 1; color: Qt.rgba(1, 1, 1, 0.10) }

        // Item 5: Lock Screen
        Rectangle {
            width: parent.width; height: 26; radius: 5
            color: item4Mouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.3) : "transparent"
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Lock Screen"; color: "#FFFFFF"; font.pixelSize: 13 }
            MouseArea { id: item4Mouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (typeof bridge !== "undefined") { bridge.launchApp("tinexus-lock"); bridge.closeAllFlyouts(); } } }
        }

        // Item 6: Sleep (Restored)
        Rectangle {
            width: parent.width; height: 26; radius: 5
            color: itemSleepMouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.3) : "transparent"
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Sleep"; color: "#FFFFFF"; font.pixelSize: 13 }
            MouseArea { id: itemSleepMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (typeof bridge !== "undefined") { bridge.powerSleep(); bridge.closeAllFlyouts(); } } }
        }

        // Divider
        Rectangle { width: parent.width - 16; anchors.horizontalCenter: parent.horizontalCenter; height: 1; color: Qt.rgba(1, 1, 1, 0.10) }

        // Item 7: Restart... (Confirmation dialog)
        Rectangle {
            width: parent.width; height: 26; radius: 5
            color: item5Mouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.3) : "transparent"
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Restart..."; color: "#FFFFFF"; font.pixelSize: 13 }
            MouseArea { id: item5Mouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (typeof bridge !== "undefined") { bridge.requestReboot(); } } }
        }

        // Item 8: Shut Down... (Confirmation dialog)
        Rectangle {
            width: parent.width; height: 26; radius: 5
            color: item6Mouse.containsMouse ? Qt.rgba(0.42, 0.55, 0.94, 0.3) : "transparent"
            Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: "Shut Down..."; color: "#FFFFFF"; font.pixelSize: 13 }
            MouseArea { id: item6Mouse; anchors.fill: parent; hoverEnabled: true; onClicked: { if (typeof bridge !== "undefined") { bridge.requestShutdown(); } } }
        }
    }
}
