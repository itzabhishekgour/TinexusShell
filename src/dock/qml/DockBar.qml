// ============================================================================
// DockBar.qml — Main Dock Component with Frosted Glass Pill & Tooltip
// ============================================================================
import QtQuick
import QtQuick.Window

Window {
    id: rootWindow

    width: 800
    height: 120
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    onWidthChanged: {
        if (typeof bridge !== "undefined") {
            bridge.updateLayout(width);
        }
    }

    Component.onCompleted: {
        if (typeof bridge !== "undefined") {
            bridge.updateLayout(width);
        }
    }

    Item {
        id: rootContainer
        anchors.fill: parent

        readonly property real pillHeight: typeof bridge !== "undefined" ? bridge.pillHeight : 56.0
        readonly property real pillWidth:  typeof bridge !== "undefined" ? bridge.pillWidth  : 300.0
        readonly property real pillY:      rootContainer.height - pillHeight - (typeof bridge !== "undefined" ? bridge.dockBotMargin : 8.0)
        readonly property real pillX:      (rootContainer.width - pillWidth) / 2.0

        // ── Drop Shadow Layers ──────────────────────────────────────
        Rectangle {
            x: rootContainer.pillX - 4
            y: rootContainer.pillY + 10
            width: rootContainer.pillWidth + 8
            height: rootContainer.pillHeight
            radius: 18
            color: Qt.rgba(0, 0, 0, 0.20)
        }

        Rectangle {
            x: rootContainer.pillX - 2
            y: rootContainer.pillY + 5
            width: rootContainer.pillWidth + 4
            height: rootContainer.pillHeight
            radius: 18
            color: Qt.rgba(0, 0, 0, 0.14)
        }

        // ── Pill Container (Frosted Glass) ──────────────────────────
        Rectangle {
            id: pillRect
            x: rootContainer.pillX
            y: rootContainer.pillY
            width: rootContainer.pillWidth
            height: rootContainer.pillHeight
            radius: 18

            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: Qt.rgba(0.11, 0.12, 0.18, 0.85) } // rgba(28, 30, 46, 215)
                GradientStop { position: 1.0; color: Qt.rgba(0.07, 0.07, 0.12, 0.90) } // rgba(18, 19, 30, 230)
            }

            border.color: Qt.rgba(1, 1, 1, 0.16)
            border.width: 1
        }

        // ── Hover Detection Area (Underneath icons in visual stacking) ──────
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton // let clicks pass through to icons

            onPositionChanged: function(mouse) {
                if (typeof bridge !== "undefined") {
                    bridge.handleHover(mouse.x);
                }
            }

            onExited: {
                if (typeof bridge !== "undefined") {
                    bridge.resetHover();
                }
            }
        }

        // ── Icon Row (Above hover detection area) ────────────────────
        Repeater {
            model: typeof bridge !== "undefined" ? bridge.icons : []

            delegate: DockIcon {
                z: 10
                readonly property var itemData: modelData
                appId: itemData.appId
                label: itemData.label
                iconType: itemData.iconType
                appState: itemData.appState
                scaleFactor: itemData.scale
                bounceOffset: itemData.bounceOffset
                isHovered: (typeof bridge !== "undefined" && bridge.hoveredIndex === index)

                // Anchor bottom of icon to the pill's content baseline
                x: itemData.centerX - width / 2.0
                y: rootContainer.pillY + rootContainer.pillHeight - (typeof bridge !== "undefined" ? bridge.dockPad : 8.0) - height

                onClicked: {
                    if (typeof bridge !== "undefined") {
                        bridge.onIconClicked(index);
                    }
                }
            }
        }

        // ── Floating Tooltip Pill ───────────────────────────────────
        Item {
            id: tooltip
            visible: typeof bridge !== "undefined" && bridge.hoveredIndex >= 0 && bridge.hoveredIndex < bridge.icons.length

            readonly property var hoveredItem: visible ? bridge.icons[bridge.hoveredIndex] : null
            readonly property string tooltipText: hoveredItem ? hoveredItem.label : ""
            readonly property real tooltipX: hoveredItem ? hoveredItem.centerX : 0.0
            readonly property real iconTopY: hoveredItem ? (rootContainer.pillY + rootContainer.pillHeight - 8.0 - (40.0 * hoveredItem.scale)) : 0.0

            width: tooltipTextMetric.width + 20.0
            height: 24.0
            x: tooltipX - width / 2.0
            y: iconTopY - height - 6.0

            Rectangle {
                anchors.fill: parent
                radius: 6.0
                color: Qt.rgba(0.08, 0.08, 0.14, 0.90) // rgba(20, 20, 35, 220)
                border.color: Qt.rgba(1, 1, 1, 0.14)
                border.width: 1

                Text {
                    id: tooltipLabel
                    anchors.centerIn: parent
                    text: tooltip.tooltipText
                    color: Qt.rgba(1, 1, 1, 0.90)
                    font.pixelSize: 12
                    font.bold: false
                }
            }

            TextMetrics {
                id: tooltipTextMetric
                font: tooltipLabel.font
                text: tooltip.tooltipText
            }
        }
    }
}
