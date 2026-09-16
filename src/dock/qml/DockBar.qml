// ============================================================================
// DockBar.qml — Tinexus Dock: LiquidGlass Pill with Pure QML Fisheye & Auto-Hide
// Slice 4: Pure declarative QML fisheye bindings, 3-state Auto-Hide machine
//          (VISIBLE, PEAKING, HIDDEN), 300ms debounce timer, YAnimator sliding.
// Ref: Architecture Blueprint §5.1-5.2, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
import QtQuick
import QtQuick.Window

Window {
    id: rootWindow

    // ── Window geometry ─────────────────────────────────────────────────────
    visible: true
    width:  Screen.desktopAvailableWidth > 0 ? Screen.desktopAvailableWidth : 1920
    height: dockConstants.windowHeight
    color:  "transparent"
    flags:  Qt.FramelessWindowHint

    // Notify C++ of window width and screen geometry
    onWidthChanged: {
        if (typeof bridge !== "undefined") {
            bridge.updateLayout(width)
            bridge.updateScreenGeometry(width, Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 1080)
        }
    }
    Component.onCompleted: {
        if (typeof bridge !== "undefined") {
            bridge.updateLayout(width)
            bridge.updateScreenGeometry(width, Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 1080)
        }
    }

    // ── Design constants ────────────────────────────────────────────────────
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
    property real globalMouseX: -1.0
    property real globalMouseY: -1.0
    property bool mouseInDock:  false

    // Derived pill geometry from bridge
    readonly property real pillWidth: typeof bridge !== "undefined" ? bridge.pillWidth : 380.0
    readonly property real pillX:     (width - pillWidth) / 2.0
    readonly property real pillY:     height - dockConstants.pillHeight - dockConstants.botMargin

    // ── Auto-Hide State Machine ─────────────────────────────────────────────
    readonly property bool autoHide: typeof bridge !== "undefined" ? bridge.autoHideEnabled : false
    property string autoHideState:   (typeof bridge !== "undefined" && bridge.autoHideState === 2) ? "HIDDEN" : "VISIBLE"

    // ── Slice 5: Drag Rearrange & Poof State ────────────────────────────────
    property bool   isDraggingIcon:  false
    property int    draggedIndex:    -1
    property string draggedAppId:    ""
    property string draggedLabel:    ""
    property string draggedIconType: "terminal"
    property real   dragX:           0.0
    property real   dragY:           0.0
    property bool   isPoofCandidate: false
    readonly property real poofThresholdPx: 80.0

    // Debounce timer (300ms) to prevent flicker on accidental overshoots
    Timer {
        id: hideDebounceTimer
        interval: 300
        repeat:   false
        onTriggered: {
            if (rootWindow.autoHide && !rootWindow.mouseInDock) {
                rootWindow.autoHideState = "HIDDEN"
                if (typeof bridge !== "undefined") bridge.setAutoHideState(2)
            }
        }
    }

    // React to C++ bridge commands (e.g. edge tripwire contact)
    Connections {
        target: typeof bridge !== "undefined" ? bridge : null
        function onRevealRequested() {
            hideDebounceTimer.stop()
            rootWindow.autoHideState = "VISIBLE"
            if (typeof bridge !== "undefined") bridge.setAutoHideState(0)
        }
        function onHideRequested() {
            rootWindow.autoHideState = "HIDDEN"
            if (typeof bridge !== "undefined") bridge.setAutoHideState(2)
        }
        function onAutoHideEnabledChanged(enabled) {
            if (!enabled) {
                hideDebounceTimer.stop()
                rootWindow.autoHideState = "VISIBLE"
            }
        }
    }

    // ── Root container ──────────────────────────────────────────────────────
    Item {
        id: rootItem
        anchors.fill: parent

        // ────────────────────────────────────────────────────────────────────
        // 1. Single Global MouseArea — tracks pointer across entire dock
        // ────────────────────────────────────────────────────────────────────
        MouseArea {
            id: globalHoverArea
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton

            onPositionChanged: function(mouse) {
                rootWindow.globalMouseX = mouse.x
                rootWindow.globalMouseY = mouse.y

                const inPillBounds = (mouse.x >= rootWindow.pillX - 20 &&
                                      mouse.x <= rootWindow.pillX + rootWindow.pillWidth + 20 &&
                                      mouse.y >= rootWindow.pillY - 30 &&
                                      mouse.y <= rootWindow.height)

                rootWindow.mouseInDock = inPillBounds

                // Cancel hide debounce if pointer is active in dock
                hideDebounceTimer.stop()

                // If currently hidden or peaking, reveal immediately on contact
                if (rootWindow.autoHide && (rootWindow.autoHideState === "HIDDEN" || rootWindow.autoHideState === "PEAKING")) {
                    rootWindow.autoHideState = "VISIBLE"
                    if (typeof bridge !== "undefined") bridge.setAutoHideState(0)
                }

                if (typeof bridge !== "undefined") bridge.handleHover(mouse.x)
            }

            onExited: {
                rootWindow.globalMouseX = -1.0
                rootWindow.globalMouseY = -1.0
                rootWindow.mouseInDock  = false
                if (typeof bridge !== "undefined") bridge.resetHover()

                // Start 300ms hide debounce timer when pointer exits
                if (rootWindow.autoHide) {
                    hideDebounceTimer.restart()
                }
            }
        }

        // ────────────────────────────────────────────────────────────────────
        // 2. Hardware-Accelerated Sliding Pill Container (YAnimator)
        // ────────────────────────────────────────────────────────────────────
        Item {
            id: pillContainer
            anchors.left:  parent.left
            anchors.right: parent.right
            height:        parent.height
            y:             0

            // ── Auto-Hide States & GPU YAnimator Transitions ────────────────
            states: [
                State {
                    name: "VISIBLE"
                    when: !rootWindow.autoHide || rootWindow.autoHideState === "VISIBLE"
                    PropertyChanges { target: pillContainer; y: 0 }
                },
                State {
                    name: "PEAKING"
                    when: rootWindow.autoHide && rootWindow.autoHideState === "PEAKING"
                    PropertyChanges { target: pillContainer; y: dockConstants.pillHeight + dockConstants.botMargin - 8 }
                },
                State {
                    name: "HIDDEN"
                    when: rootWindow.autoHide && rootWindow.autoHideState === "HIDDEN"
                    PropertyChanges { target: pillContainer; y: dockConstants.pillHeight + dockConstants.botMargin + 30 }
                }
            ]

            transitions: [
                Transition {
                    from: "VISIBLE"; to: "HIDDEN"
                    YAnimator {
                        target:   pillContainer
                        duration: 280
                        easing.type: Easing.InCubic
                    }
                },
                Transition {
                    from: "HIDDEN"; to: "VISIBLE"
                    YAnimator {
                        target:   pillContainer
                        duration: 240
                        easing.type: Easing.OutCubic
                    }
                },
                Transition {
                    from: "*"; to: "PEAKING"
                    YAnimator {
                        target:   pillContainer
                        duration: 160
                        easing.type: Easing.OutQuad
                    }
                }
            ]

            // ── Drop Shadows ────────────────────────────────────────────────
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

            // ── LiquidGlass Pill Body ───────────────────────────────────────
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

            // Top specular highlight (11% shimmer)
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

            // Inner bottom edge glow
            Rectangle {
                x:      rootWindow.pillX + 2
                y:      rootWindow.pillY + dockConstants.pillHeight - 3
                width:  rootWindow.pillWidth - 4
                height: 3
                radius: 2
                color:  Qt.rgba(1.0, 1.0, 1.0, 0.06)
            }

            // ────────────────────────────────────────────────────────────────
            // 3. Icon strip — Pure QML declarative fisheye math
            // ────────────────────────────────────────────────────────────────
            Repeater {
                id: iconRepeater
                model: typeof bridge !== "undefined" ? bridge.icons : []

                delegate: DockItem {
                    id: iconDelegate
                    z: 10

                    // Data from DockBridge QVariantList
                    readonly property var  md:        modelData
                    readonly property real bounceOff: md.bounceOffset
                    readonly property bool isSep:     md.iconType === "separator"

                    // Pure QML Fisheye Math Inputs
                    cursorX:       rootWindow.globalMouseX
                    mouseInDock:   rootWindow.mouseInDock
                    centerX:       md.centerX

                    // Slice 5: Drag & Rearrange binding
                    itemIndex:      index
                    isBeingDragged: rootWindow.isDraggingIcon && rootWindow.draggedIndex === index

                    // Geometry
                    readonly property real baseSize: dockConstants.baseSize
                    readonly property real iconSize: baseSize * scaleFactor
                    readonly property real iconBaseY: rootWindow.pillY + dockConstants.pillHeight
                                                     - dockConstants.dockPad - baseSize
                    readonly property real liftY: iconBaseY - (iconSize - baseSize) - (bounceOff * 40.0)

                    visible: true
                    x: isSep ? (centerX - dockConstants.separatorWidth / 2.0) : (centerX - iconSize / 2.0)
                    y: isSep ? (rootWindow.pillY + (dockConstants.pillHeight - dockConstants.separatorHeight) / 2.0)
                              : liftY
                    width:  isSep ? dockConstants.separatorWidth : iconSize
                    height: isSep ? dockConstants.separatorHeight : iconSize

                    // Smooth gap shifting when neighbor items move
                    Behavior on x {
                        enabled: !rootWindow.isDraggingIcon || rootWindow.draggedIndex !== index
                        NumberAnimation {
                            duration: 180
                            easing.type: Easing.OutCubic
                        }
                    }

                    // Separator override: thin line
                    Rectangle {
                        anchors.fill: parent
                        visible:      iconDelegate.isSep
                        color:        Qt.rgba(1, 1, 1, 0.20)
                        radius:       1
                    }

                    // Forwarded item properties
                    appId:         md.appId
                    label:         md.label
                    iconType:      md.iconType
                    appState:      md.appState
                    isHovered:     typeof bridge !== "undefined" && bridge.hoveredIndex === index && !rootWindow.isDraggingIcon
                    badgeCount:    md.badgeCount !== undefined ? md.badgeCount : 0
                    isRunning:     md.isRunning !== undefined ? md.isRunning : (md.appState > 0)
                    isActive:      md.isActive !== undefined ? md.isActive : (md.appState === 1 || md.appState === 2)
                    toplevelCount: md.toplevelCount !== undefined ? md.toplevelCount : (md.appState > 0 ? 1 : 0)
                    isDropTarget:  md.isDropTarget !== undefined ? md.isDropTarget : (typeof bridge !== "undefined" && bridge.dropTargetIndex === index)

                    onClicked: {
                        if (typeof bridge !== "undefined") bridge.onIconClicked(index)
                    }
                    onContextMenuRequested: {
                        if (typeof bridge !== "undefined") bridge.requestContextMenu(index)
                    }

                    // Slice 5: Drag gesture handlers
                    onDragStarted: function(idx, gx, gy, appId) {
                        if (md.iconType === "separator" || md.iconType === "trash") return
                        rootWindow.isDraggingIcon  = true
                        rootWindow.draggedIndex    = idx
                        rootWindow.draggedAppId    = appId
                        rootWindow.draggedLabel    = md.label
                        rootWindow.draggedIconType = md.iconType
                        rootWindow.dragX           = gx
                        rootWindow.dragY           = gy
                        rootWindow.isPoofCandidate = false
                    }

                    onDragMoved: function(gx, gy) {
                        if (!rootWindow.isDraggingIcon) return
                        rootWindow.dragX = gx
                        rootWindow.dragY = gy

                        // 80px upwards threshold for Poof
                        if (gy < rootWindow.pillY - rootWindow.poofThresholdPx) {
                            rootWindow.isPoofCandidate = true
                        } else {
                            rootWindow.isPoofCandidate = false

                            // Real-time rearrange among pinned items (before separator)
                            if (typeof bridge !== "undefined" && bridge.icons) {
                                let bestIndex = rootWindow.draggedIndex
                                let bestDist = 999999.0
                                for (let i = 0; i < bridge.icons.length; ++i) {
                                    const item = bridge.icons[i]
                                    if (item.iconType === "separator" || item.iconType === "trash") break
                                    const dist = Math.abs(gx - item.centerX)
                                    if (dist < bestDist) {
                                        bestDist = dist
                                        bestIndex = i
                                    }
                                }

                                if (bestIndex !== rootWindow.draggedIndex && bestIndex >= 0) {
                                    if (typeof bridge.moveItem === "function") {
                                        bridge.moveItem(rootWindow.draggedIndex, bestIndex)
                                    } else if (typeof dockModel !== "undefined" && typeof dockModel.moveItem === "function") {
                                        dockModel.moveItem(rootWindow.draggedIndex, bestIndex)
                                    }
                                    rootWindow.draggedIndex = bestIndex
                                }
                            }
                        }
                    }

                    onDragFinished: function(gx, gy) {
                        if (!rootWindow.isDraggingIcon) return
                        if (rootWindow.isPoofCandidate) {
                            poofEffect.trigger(gx, gy, rootWindow.draggedAppId)
                        } else {
                            if (typeof bridge !== "undefined" && typeof bridge.commitMove === "function") {
                                bridge.commitMove()
                            } else if (typeof dockModel !== "undefined" && typeof dockModel.commitMove === "function") {
                                dockModel.commitMove()
                            }
                        }
                        rootWindow.isDraggingIcon  = false
                        rootWindow.draggedIndex    = -1
                        rootWindow.isPoofCandidate = false
                    }

                    onDragCanceled: {
                        rootWindow.isDraggingIcon  = false
                        rootWindow.draggedIndex    = -1
                        rootWindow.isPoofCandidate = false
                    }
                }
            }

            // ────────────────────────────────────────────────────────────────
            // 4. Floating tooltip pill — premium glass style
            // ────────────────────────────────────────────────────────────────
            Item {
                id: tooltip
                z: 100

                readonly property bool shouldShow: typeof bridge !== "undefined"
                                                   && bridge.hoveredIndex >= 0
                                                   && bridge.hoveredIndex < bridge.icons.length
                                                   && bridge.icons[bridge.hoveredIndex].iconType !== "separator"
                                                   && rootWindow.mouseInDock
                                                   && rootWindow.autoHideState === "VISIBLE"

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
                    text: tooltip.tipText
                }
            }
        }

        // ────────────────────────────────────────────────────────────────────
        // 5. Floating Dragged Icon Clone (Slice 5)
        // ────────────────────────────────────────────────────────────────────
        Item {
            id: floatingClone
            z: 9999
            visible: rootWindow.isDraggingIcon
            width:   dockConstants.baseSize * 1.15
            height:  dockConstants.baseSize * 1.15
            x:       rootWindow.dragX - width / 2.0
            y:       rootWindow.dragY - height / 2.0

            scale:   rootWindow.isPoofCandidate ? 0.85 : 1.15
            opacity: rootWindow.isPoofCandidate ? 0.60 : 0.95

            Behavior on scale   { SmoothedAnimation { velocity: 8.0 } }
            Behavior on opacity { SmoothedAnimation { velocity: 8.0 } }

            // Shadow
            Rectangle {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: 12
                width: parent.width + 10
                height: parent.height + 10
                radius: 16
                color: Qt.rgba(0, 0, 0, 0.40)
            }

            // LiquidGlass Icon Tile
            Rectangle {
                anchors.fill: parent
                radius: 13
                clip: true
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop {
                        position: 0.0
                        color: {
                            switch (rootWindow.draggedIconType) {
                            case "folder":   return "#2B7DE9"
                            case "gear":     return "#6B6B70"
                            case "barchart": return "#0E1520"
                            case "firefox":  return "#E8450A"
                            case "package":  return "#0A7DFF"
                            case "trash":    return "#3A3A3C"
                            default:         return "#131625"
                            }
                        }
                    }
                    GradientStop {
                        position: 1.0
                        color: {
                            switch (rootWindow.draggedIconType) {
                            case "folder":   return "#0048B8"
                            case "gear":     return "#48484A"
                            case "barchart": return "#070B10"
                            case "firefox":  return "#6425C8"
                            case "package":  return "#004FCC"
                            case "trash":    return "#1C1C1E"
                            default:         return "#090B14"
                            }
                        }
                    }
                }
                border.color: Qt.rgba(1.0, 1.0, 1.0, 0.30)
                border.width: 1.5

                // Specular highlight
                Rectangle {
                    x: 1; y: 1
                    width: parent.width - 2
                    height: parent.height * 0.42
                    radius: 12
                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.20) }
                        GradientStop { position: 1.0; color: Qt.rgba(1, 1, 1, 0.00) }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: {
                        switch (rootWindow.draggedIconType) {
                        case "terminal": return ">_"
                        case "gear":     return "⚙"
                        case "package":  return "A"
                        default:         return rootWindow.draggedLabel.length > 0 ? rootWindow.draggedLabel.substring(0, 1) : "★"
                        }
                    }
                    font.pixelSize: 22
                    font.bold: true
                    color: rootWindow.draggedIconType === "terminal" ? "#2ECC6A" : "#FFFFFF"
                }
            }
        }

        // ────────────────────────────────────────────────────────────────────
        // 6. Poof Particle Burst Effect (Slice 5)
        // ────────────────────────────────────────────────────────────────────
        PoofEffect {
            id: poofEffect
            onFinished: function(appId) {
                if (typeof bridge !== "undefined" && typeof bridge.unpinApp === "function") {
                    bridge.unpinApp(appId)
                } else if (typeof dockModel !== "undefined" && typeof dockModel.unpinApp === "function") {
                    dockModel.unpinApp(appId)
                }
            }
        }
    }
}
