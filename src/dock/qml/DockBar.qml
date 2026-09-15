// ============================================================================
// DockBar.qml — Tinexus Dock: LiquidGlass Pill with Fisheye Magnification
// Slice 1: Static pinned dock strip, frosted glass, running indicators, tooltip.
// Ref: Architecture Blueprint §5.1-5.4, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
import QtQuick
import QtQuick.Window
import QtQuick.Effects

Window {
    id: rootWindow

    // ── Window geometry ─────────────────────────────────────────────────────
    // The window covers the full screen width but is tall enough to
    // accommodate the fisheye magnification lift (icons grow upward).
    width:  Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 1920
    height: dockConstants.windowHeight
    color:  "transparent"
    flags:  Qt.FramelessWindowHint

    // Notify C++ of window width so DockBridge can compute pill geometry
    onWidthChanged: {
        if (typeof bridge !== "undefined") bridge.updateLayout(width)
    }
    Component.onCompleted: {
        if (typeof bridge !== "undefined") bridge.updateLayout(width)
    }

    // ── Design constants (single source of truth) ────────────────────────────
    QtObject {
        id: dockConstants
        readonly property real baseSize:       typeof bridge !== "undefined" ? bridge.baseSize : 48.0
        readonly property real gap:            typeof bridge !== "undefined" ? bridge.gap      : 10.0
        readonly property real dockPad:        typeof bridge !== "undefined" ? bridge.dockPad  : 10.0
        readonly property real botMargin:      typeof bridge !== "undefined" ? bridge.dockBotMargin : 8.0
        readonly property real pillRadius:     20.0
        readonly property real iconRadius:     11.0
        readonly property real pillHeight:     baseSize + dockPad * 2.0          // 68px
        readonly property real windowHeight:   pillHeight + baseSize * 0.68 + 40 // headroom for fisheye
        readonly property real separatorWidth: 1.0
        readonly property real separatorHeight: baseSize * 0.55
    }

    // ── Mouse tracking (single MouseArea drives ALL fisheye) ─────────────────
    property real  globalMouseX:   -1.0
    property bool  mouseInDock:    false
    property int   hoveredIndex:   -1

    // Derived pill geometry from bridge
    readonly property real pillWidth:  typeof bridge !== "undefined" ? bridge.pillWidth : 380.0
    readonly property real pillX:      (width - pillWidth) / 2.0
    readonly property real pillY:      height - dockConstants.pillHeight - dockConstants.botMargin

    // ── Root item ────────────────────────────────────────────────────────────
    Item {
        id: rootItem
        anchors.fill: parent

        // ────────────────────────────────────────────────────────────────────
        // 1. Multi-layer drop shadows beneath the pill
        // ────────────────────────────────────────────────────────────────────
        Rectangle {
            x:      rootWindow.pillX - 6
            y:      rootWindow.pillY + 14
            width:  rootWindow.pillWidth + 12
            height: dockConstants.pillHeight
            radius: dockConstants.pillRadius + 2
            color:  Qt.rgba(0, 0, 0, 0.26)
        }
        Rectangle {
            x:      rootWindow.pillX - 3
            y:      rootWindow.pillY + 7
            width:  rootWindow.pillWidth + 6
            height: dockConstants.pillHeight
            radius: dockConstants.pillRadius + 1
            color:  Qt.rgba(0, 0, 0, 0.16)
        }

        // ────────────────────────────────────────────────────────────────────
        // 2. LiquidGlass Pill Body
        // Layered approach: dark frosted base + top specular rim + inner glow
        // ────────────────────────────────────────────────────────────────────
        Rectangle {
            id: pillBody
            x:      rootWindow.pillX
            y:      rootWindow.pillY
            width:  rootWindow.pillWidth
            height: dockConstants.pillHeight
            radius: dockConstants.pillRadius
            clip:   false

            // Frosted dark glass base
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0;  color: Qt.rgba(0.085, 0.090, 0.140, 0.88) }
                GradientStop { position: 0.45; color: Qt.rgba(0.065, 0.068, 0.110, 0.90) }
                GradientStop { position: 1.0;  color: Qt.rgba(0.040, 0.042, 0.075, 0.94) }
            }

            // Outer glass border
            border.color: Qt.rgba(1.0, 1.0, 1.0, 0.18)
            border.width: 1
        }

        // Top specular highlight (liquid-glass shimmer)
        Rectangle {
            x:      rootWindow.pillX + 1
            y:      rootWindow.pillY + 1
            width:  rootWindow.pillWidth - 2
            height: dockConstants.pillHeight * 0.40
            radius: dockConstants.pillRadius - 1
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: Qt.rgba(1.0, 1.0, 1.0, 0.11) }
                GradientStop { position: 1.0; color: Qt.rgba(1.0, 1.0, 1.0, 0.00) }
            }
        }

        // Inner bottom edge glow (depth illusion)
        Rectangle {
            x:      rootWindow.pillX + 2
            y:      rootWindow.pillY + dockConstants.pillHeight - 3
            width:  rootWindow.pillWidth - 4
            height: 3
            radius: 2
            color:  Qt.rgba(1.0, 1.0, 1.0, 0.06)
        }

        // ────────────────────────────────────────────────────────────────────
        // 3. Global mouse tracking — drives ALL fisheye without per-icon timers
        // ────────────────────────────────────────────────────────────────────
        MouseArea {
            id: globalHoverArea
            x:      rootWindow.pillX
            y:      0
            width:  rootWindow.pillWidth
            height: rootWindow.height
            hoverEnabled:    true
            acceptedButtons: Qt.NoButton

            onPositionChanged: function(mouse) {
                rootWindow.globalMouseX = mouse.x + rootWindow.pillX
                rootWindow.mouseInDock  = true
                if (typeof bridge !== "undefined") bridge.handleHover(rootWindow.globalMouseX)
            }
            onExited: {
                rootWindow.globalMouseX = -1.0
                rootWindow.mouseInDock  = false
                if (typeof bridge !== "undefined") bridge.resetHover()
            }
        }

        // ────────────────────────────────────────────────────────────────────
        // 4. Icon strip — Repeater over bridge.icons (DockBridge QVariantList)
        //    Each icon computes its own fisheye scale as a pure binding.
        // ────────────────────────────────────────────────────────────────────
        Repeater {
            id: iconRepeater
            model: typeof bridge !== "undefined" ? bridge.icons : []

            delegate: DockItem {
                id: iconDelegate
                z: 10

                // Data from DockBridge QVariantList
                readonly property var  md:           modelData
                readonly property real scaleFactor:  md.scale
                readonly property real bounceOff:    md.bounceOffset
                readonly property bool isSep:        md.iconType === "separator"

                // Fisheye geometry
                readonly property real baseSize:     dockConstants.baseSize
                readonly property real iconSize:     baseSize * scaleFactor
                readonly property real centerX:      md.centerX
                readonly property real iconBaseY:    rootWindow.pillY + dockConstants.pillHeight
                                                     - dockConstants.dockPad - baseSize
                readonly property real liftY:        iconBaseY - (iconSize - baseSize) - (bounceOff * 40.0)

                // ── Separator rendering (not a DockItem, just a line) ──────
                visible: true
                x: isSep ? (centerX - dockConstants.separatorWidth / 2.0) : (centerX - iconSize / 2.0)
                y: isSep ? (rootWindow.pillY + (dockConstants.pillHeight - dockConstants.separatorHeight) / 2.0)
                          : liftY
                width:  isSep ? dockConstants.separatorWidth : iconSize
                height: isSep ? dockConstants.separatorHeight : iconSize

                // Separator override: just a thin line, skip DockItem rendering
                Rectangle {
                    anchors.fill:  parent
                    visible:       iconDelegate.isSep
                    color:         Qt.rgba(1, 1, 1, 0.20)
                    radius:        1
                }

                // Icon properties forwarded
                appId:       md.appId
                label:       md.label
                iconType:    md.iconType
                appState:    md.appState
                isHovered:   typeof bridge !== "undefined" && bridge.hoveredIndex === index
                badgeCount:  md.badgeCount !== undefined ? md.badgeCount : 0
                isRunning:   md.appState > 0
                isActive:    md.appState === 2
                visible:     !isSep

                onClicked: {
                    if (typeof bridge !== "undefined") bridge.onIconClicked(index)
                }
            }
        }

        // ────────────────────────────────────────────────────────────────────
        // 5. Separator lines — rendered inline above as Rectangle overlays
        //    (see DockItem delegate isSep branch above)
        // ────────────────────────────────────────────────────────────────────

        // ────────────────────────────────────────────────────────────────────
        // 6. Floating tooltip pill — premium glass style
        // ────────────────────────────────────────────────────────────────────
        Item {
            id: tooltip
            z: 100

            readonly property bool shouldShow: typeof bridge !== "undefined"
                                               && bridge.hoveredIndex >= 0
                                               && bridge.hoveredIndex < bridge.icons.length
                                               && bridge.icons[bridge.hoveredIndex].iconType !== "separator"

            readonly property var  hovered:     shouldShow ? bridge.icons[bridge.hoveredIndex] : null
            readonly property string tipText:   hovered ? hovered.label : ""
            readonly property real  tipCenterX: hovered ? hovered.centerX : 0.0
            readonly property real  iconTopY:   hovered
                ? (rootWindow.pillY + dockConstants.pillHeight
                   - dockConstants.dockPad
                   - dockConstants.baseSize * hovered.scale)
                : 0.0

            width:   tooltipMetrics.width + 24.0
            height:  28.0
            x:       tipCenterX - width / 2.0
            y:       iconTopY - height - 8.0
            visible: shouldShow

            Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            opacity: shouldShow ? 1.0 : 0.0

            // Tooltip glass pill
            Rectangle {
                anchors.fill: parent
                radius:       8.0
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: Qt.rgba(0.10, 0.10, 0.18, 0.92) }
                    GradientStop { position: 1.0; color: Qt.rgba(0.06, 0.06, 0.12, 0.95) }
                }
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.18)
                border.width: 1

                // Specular top rim
                Rectangle {
                    x: 1; y: 1
                    width:  parent.width - 2
                    height: parent.height * 0.40
                    radius: 7
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: Qt.rgba(1,1,1,0.10) }
                        GradientStop { position: 1.0; color: Qt.rgba(1,1,1,0.00) }
                    }
                }

                Text {
                    id: tooltipLabel
                    anchors.centerIn: parent
                    text:            tooltip.tipText
                    color:           Qt.rgba(1, 1, 1, 0.92)
                    font.pixelSize:  12
                    font.family:     "Inter, SF Pro Display, sans-serif"
                    font.weight:     Font.Medium
                    renderType:      Text.QtRendering
                }
            }

            TextMetrics {
                id:   tooltipMetrics
                font: tooltipLabel.font
                text: tooltip.tooltipText
            }
        }
    }
}
