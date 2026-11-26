// ============================================================================
// DockBar.qml — Tinexus Dock: LiquidGlass Pill with Pure QML Fisheye & Auto-Hide
// Slice 4: Pure declarative QML fisheye bindings, 3-state Auto-Hide machine
//          (VISIBLE, PEAKING, HIDDEN), 300ms debounce timer, YAnimator sliding.
// Ref: Architecture Blueprint §5.1-5.2, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
import QtQuick
import QtQuick.Window
import "../../common/qml"

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
            bridge.updateScreenGeometry(width,
                                        Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 1080,
                                        Screen.virtualX !== undefined ? Screen.virtualX : 0,
                                        Screen.virtualY !== undefined ? Screen.virtualY : 0)
        }
    }
    Component.onCompleted: {
        if (typeof bridge !== "undefined") {
            bridge.updateLayout(width)
            bridge.updateScreenGeometry(width,
                                        Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 1080,
                                        Screen.virtualX !== undefined ? Screen.virtualX : 0,
                                        Screen.virtualY !== undefined ? Screen.virtualY : 0)
        }
        syncInputRegion()
        syncBlurRegion()
        if (rootWindow.autoHide && !rootWindow.mouseInDock) {
            hideDebounceTimer.restart()
        }
    }

    // ── Design constants ────────────────────────────────────────────────────
    QtObject {
        id: dockConstants
        readonly property real baseSize:       typeof bridge !== "undefined" ? bridge.baseSize : 48.0
        readonly property real gap:            typeof bridge !== "undefined" ? bridge.gap      : 14.0
        readonly property real dockPad:        typeof bridge !== "undefined" ? bridge.dockPad  : 16.0
        readonly property real botMargin:      typeof bridge !== "undefined" ? bridge.dockBotMargin : 14.0
        readonly property real pillRadius:     22.0
        readonly property real iconRadius:     11.5
        readonly property real pillHeight:     typeof bridge !== "undefined" ? bridge.pillHeight : 68.0
        readonly property real windowHeight:   pillHeight + baseSize * 0.85 + 64 // headroom for fisheye, tooltip, & bottom shadow
        readonly property real separatorWidth: 1.0
        readonly property real separatorHeight: baseSize * 0.55
    }

    // ── Mouse tracking (single MouseArea drives ALL fisheye) ─────────────────
    property real globalMouseX: (typeof bridge !== "undefined" && bridge.hoveredIndex >= 0 && bridge.hoveredIndex < bridge.icons.length) ? bridge.icons[bridge.hoveredIndex].centerX : -1.0
    property real globalMouseY: -1.0
    property bool mouseInDock:  (typeof bridge !== "undefined" && bridge.hoveredIndex >= 0)

    // Derived pill geometry from bridge
    readonly property real pillWidth: typeof bridge !== "undefined" ? bridge.pillWidth : 380.0
    readonly property real pillX:     (width - pillWidth) / 2.0
    readonly property real pillY:     height - dockConstants.pillHeight - dockConstants.botMargin

    // Synchronize native Wayland input region mask to the pill geometry
    function syncInputRegion() {
        if (typeof dockWindow === "undefined" || !dockWindow) return;
        if (rootWindow.isDraggingIcon) {
            dockWindow.updateInputRegion(0, 0, Math.ceil(rootWindow.width), Math.ceil(rootWindow.height));
        } else if (rootWindow.autoHideState === "HIDDEN") {
            const sx = Math.max(0, Math.floor(rootWindow.pillX));
            const sw = Math.max(120, Math.ceil(rootWindow.pillWidth));
            dockWindow.updateInputRegion(sx, Math.max(0, rootWindow.height - 2), sw, 2);
        } else {
            const rx = Math.max(0, Math.floor(rootWindow.pillX - 10));
            const ry = Math.max(0, Math.floor(rootWindow.pillY - dockConstants.baseSize * 0.85 - 25));
            const rw = Math.ceil(rootWindow.pillWidth + 20);
            const rh = Math.ceil(rootWindow.height - ry);
            dockWindow.updateInputRegion(rx, ry, rw, rh);
        }
    }

    // Synchronize tight pill Dual-Kawase blur geometry separately from taller hover input region
    function syncBlurRegion() {
        if (typeof dockWindow === "undefined" || !dockWindow || typeof dockWindow.updateBlurRegion !== "function") return;
        const visible = (rootWindow.autoHideState !== "HIDDEN") && (rootWindow.pillWidth > 0);
        const curY = Math.round(rootWindow.pillY + pillContainer.y);
        dockWindow.updateBlurRegion(Math.round(rootWindow.pillX), curY,
                                    Math.round(rootWindow.pillWidth), Math.round(dockConstants.pillHeight),
                                    dockConstants.pillRadius, visible);
    }

    onPillWidthChanged: { syncInputRegion(); syncBlurRegion() }
    onPillXChanged: { syncInputRegion(); syncBlurRegion() }
    onPillYChanged: { syncInputRegion(); syncBlurRegion() }
    onAutoHideStateChanged: { syncInputRegion(); syncBlurRegion() }
    onHeightChanged: { syncInputRegion(); syncBlurRegion() }
    onIsDraggingIconChanged: { syncInputRegion() }

    // ── Auto-Hide State Machine ─────────────────────────────────────────────
    readonly property bool autoHide: typeof bridge !== "undefined" ? bridge.autoHideEnabled : false
    property string autoHideState:   (typeof bridge !== "undefined" && bridge.autoHideState === 2) ? "HIDDEN" : "VISIBLE"

    // ── Slice 5: Drag Rearrange & Poof State ────────────────────────────────
    property bool   isDraggingIcon:  false
    property int    draggedIndex:    -1
    property int    dragTargetIndex: -1
    property string draggedAppId:    ""
    property string draggedLabel:    ""
    property string draggedIconType: "terminal"
    property string draggedIconUrl:  ""
    property real   dragX:           0.0
    property real   dragY:           0.0
    property bool   isPoofCandidate: false
    property bool   draggedIsRunning: false
    readonly property real poofThresholdPx: 80.0

    function cancelActiveDrag() {
        if (rootWindow.isDraggingIcon) {
            rootWindow.isDraggingIcon   = false
            rootWindow.draggedIndex     = -1
            rootWindow.dragTargetIndex  = -1
            rootWindow.isPoofCandidate  = false
            rootWindow.draggedIsRunning = false
            rootWindow.dragX            = 0.0
            rootWindow.dragY            = 0.0
        }
    }

    onActiveChanged: {
        if (!active && rootWindow.isDraggingIcon) {
            cancelActiveDrag()
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: rootWindow.isDraggingIcon
        onActivated: rootWindow.cancelActiveDrag()
    }

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
                if (typeof bridge !== "undefined") bridge.setAutoHideState(0)
            } else {
                if (!rootWindow.mouseInDock) {
                    hideDebounceTimer.restart()
                }
            }
        }
    }

    // ── Root container ──────────────────────────────────────────────────────
    Item {
        id: rootItem
        anchors.fill: parent

        // ────────────────────────────────────────────────────────────────────
        // 1. Single Scoped MouseArea — tracks pointer ONLY within pill active zone
        // ────────────────────────────────────────────────────────────────────
        MouseArea {
            id: globalHoverArea
            x: Math.max(0, rootWindow.pillX - 10)
            y: rootWindow.autoHideState === "HIDDEN"
               ? Math.max(0, rootWindow.height - 4)
               : Math.max(0, rootWindow.pillY - dockConstants.baseSize * 0.85 - 25)
            width: rootWindow.pillWidth + 20
            height: rootWindow.height - y
            hoverEnabled: true
            acceptedButtons: Qt.NoButton

            onEntered: {
                rootWindow.mouseInDock = true
                hideDebounceTimer.stop()
                if (rootWindow.autoHide && (rootWindow.autoHideState === "HIDDEN" || rootWindow.autoHideState === "PEAKING")) {
                    rootWindow.autoHideState = "VISIBLE"
                    if (typeof bridge !== "undefined") bridge.setAutoHideState(0)
                }
            }

            onPositionChanged: function(mouse) {
                // Map local coordinate to window coordinate
                const winX = x + mouse.x
                const winY = y + mouse.y
                rootWindow.globalMouseX = winX
                rootWindow.globalMouseY = winY
                rootWindow.mouseInDock = true

                // Cancel hide debounce if pointer is active in dock
                hideDebounceTimer.stop()

                // If currently hidden or peaking, reveal immediately on contact
                if (rootWindow.autoHide && (rootWindow.autoHideState === "HIDDEN" || rootWindow.autoHideState === "PEAKING")) {
                    rootWindow.autoHideState = "VISIBLE"
                    if (typeof bridge !== "undefined") bridge.setAutoHideState(0)
                }

                if (typeof bridge !== "undefined") bridge.handleHover(winX)
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
            onYChanged:    rootWindow.syncBlurRegion()

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

            // ── LiquidGlass Pill Body (Compositor Blur + MultiEffect Shadow + Specular Rim) ──
            Item {
                id: pillBody
                x:      rootWindow.pillX
                y:      rootWindow.pillY
                width:  rootWindow.pillWidth
                height: dockConstants.pillHeight

                LiquidGlass {
                    id: pillGlass
                    anchors.fill: parent
                    materialType: "popover"
                    cornerRadius: dockConstants.pillRadius
                    fluidOpacity: 0.62
                    topSpecularOpacity: 0.24
                    shadowEnabled: true
                }

                // Right-click on pill background invokes dock-level options
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: function(mouse) {
                        if (typeof bridge !== "undefined") {
                            bridge.requestContextMenu(-1, "__dock__", pillBody.x + mouse.x, rootWindow.height - dockConstants.botMargin)
                        }
                    }
                }
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

                    // Geometry (single unified source of truth from DockBridge spring math)
                    readonly property real baseSize: dockConstants.baseSize
                    baseIconSize: dockConstants.baseSize
                    externalScale: (!isSep && md.scale !== undefined) ? md.scale : 1.0
                    readonly property real iconSize: baseSize * scaleFactor
                    readonly property real iconBaseY: rootWindow.pillY + 7.0
                    readonly property real liftY: iconBaseY - (iconSize - baseSize) - (bounceOff * 38.0)

                    visible: true
                    property real slotShiftX: {
                        if (!rootWindow.isDraggingIcon || rootWindow.isPoofCandidate || isBeingDragged || isSep) {
                            return 0.0
                        }
                        const fromIdx = rootWindow.draggedIndex
                        const toIdx = rootWindow.dragTargetIndex
                        if (fromIdx < 0 || toIdx < 0 || fromIdx === toIdx) return 0.0
                        const step = dockConstants.baseSize + dockConstants.gap
                        if (toIdx > fromIdx && index > fromIdx && index <= toIdx) {
                            return -step
                        }
                        if (toIdx < fromIdx && index >= toIdx && index < fromIdx) {
                            return step
                        }
                        return 0.0
                    }
                    Behavior on slotShiftX {
                        SpringAnimation {
                            spring: 6.0
                            damping: 0.55
                            epsilon: 0.5
                        }
                    }

                    x: (isSep ? (centerX - dockConstants.separatorWidth / 2.0) : (centerX - iconSize / 2.0)) + slotShiftX
                    y: isSep ? (rootWindow.pillY + (dockConstants.pillHeight - dockConstants.separatorHeight) / 2.0)
                              : liftY
                    width:  isSep ? dockConstants.separatorWidth : iconSize
                    height: isSep ? dockConstants.separatorHeight : iconSize
                    pillFloorY: rootWindow.pillY + dockConstants.pillHeight - 9.0

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
                    iconUrl:       md.iconUrl !== undefined ? md.iconUrl : ""
                    appState:      md.appState
                    isHovered:     typeof bridge !== "undefined" && bridge.hoveredIndex === index && !rootWindow.isDraggingIcon
                    badgeCount:    md.badgeCount !== undefined ? md.badgeCount : 0
                    isRunning:     md.isRunning !== undefined ? md.isRunning : (md.appState > 0)
                    isActive:      md.isActive !== undefined ? md.isActive : (md.appState === 1)
                    needsAttention: md.needsAttention !== undefined ? md.needsAttention : false
                    toplevelCount: md.toplevelCount !== undefined ? md.toplevelCount : (md.appState > 0 ? 1 : 0)
                    isDropTarget:  md.isDropTarget !== undefined ? md.isDropTarget : (typeof bridge !== "undefined" && bridge.dropTargetIndex === index)

                    onClicked: {
                        if (typeof bridge !== "undefined") bridge.onIconClicked(index)
                    }
                    onContextMenuRequested: {
                        if (typeof bridge !== "undefined") bridge.requestContextMenu(index)
                    }

                    // Slice 5: Drag gesture handlers (live visual slot shifting without mid-drag model resets)
                    onDragStarted: function(idx, gx, gy, appId) {
                        if (md.iconType === "separator" || md.iconType === "trash") return
                        rootWindow.isDraggingIcon   = true
                        rootWindow.draggedIndex     = idx
                        rootWindow.dragTargetIndex  = idx
                        rootWindow.draggedAppId     = appId
                        rootWindow.draggedLabel     = md.label
                        rootWindow.draggedIconType  = md.iconType
                        rootWindow.draggedIconUrl   = md.iconUrl !== undefined ? md.iconUrl : ""
                        rootWindow.draggedIsRunning = Boolean(md.isRunning) || (md.toplevelCount > 0)
                        rootWindow.dragX            = gx
                        rootWindow.dragY            = gy
                        rootWindow.isPoofCandidate  = false
                    }

                    onDragMoved: function(gx, gy) {
                        if (!rootWindow.isDraggingIcon) return
                        rootWindow.dragX = gx
                        rootWindow.dragY = gy

                        // 80px upwards threshold for Poof (non-running pinned apps only)
                        if (!rootWindow.draggedIsRunning && gy < rootWindow.pillY - rootWindow.poofThresholdPx) {
                            rootWindow.isPoofCandidate = true
                            rootWindow.dragTargetIndex = rootWindow.draggedIndex
                        } else {
                            rootWindow.isPoofCandidate = false

                            // Compute live neighbor swap target slot among pinned items (before separator)
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
                                rootWindow.dragTargetIndex = bestIndex
                            }
                        }
                    }

                    onDragFinished: function(gx, gy) {
                        if (!rootWindow.isDraggingIcon) return
                        const fromIdx = rootWindow.draggedIndex
                        const toIdx = rootWindow.dragTargetIndex
                        const poof = rootWindow.isPoofCandidate
                        const appId = rootWindow.draggedAppId

                        rootWindow.isDraggingIcon   = false
                        rootWindow.draggedIndex     = -1
                        rootWindow.dragTargetIndex  = -1
                        rootWindow.isPoofCandidate  = false
                        rootWindow.draggedIsRunning = false

                        if (poof) {
                            poofEffect.trigger(gx, gy, appId)
                        } else if (fromIdx >= 0 && toIdx >= 0 && fromIdx !== toIdx) {
                            if (typeof bridge !== "undefined" && typeof bridge.moveItem === "function") {
                                bridge.moveItem(fromIdx, toIdx)
                                bridge.commitMove()
                            } else if (typeof dockModel !== "undefined" && typeof dockModel.moveItem === "function") {
                                dockModel.moveItem(fromIdx, toIdx)
                                dockModel.commitMove()
                            }
                        }
                    }

                    onDragCanceled: {
                        rootWindow.cancelActiveDrag()
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
                                                   && !rootWindow.isDraggingIcon

                readonly property var    activeItem: shouldShow && iconRepeater.count > bridge.hoveredIndex ? iconRepeater.itemAt(bridge.hoveredIndex) : null
                readonly property string tipText:    shouldShow && bridge.icons[bridge.hoveredIndex] ? bridge.icons[bridge.hoveredIndex].label : ""
                readonly property real   tipCenterX: (bridge.icons[bridge.hoveredIndex] && bridge.icons[bridge.hoveredIndex].centerX !== undefined)
                                                     ? bridge.icons[bridge.hoveredIndex].centerX
                                                     : (activeItem ? (activeItem.x + activeItem.width / 2.0) : 0.0)

                // Accurate visual top of the active squircle
                readonly property real   iconTopY: {
                    if (typeof bridge === "undefined" || bridge.hoveredIndex < 0 || !bridge.icons[bridge.hoveredIndex]) {
                        return rootWindow.pillY + 7.0
                    }
                    const iconData = bridge.icons[bridge.hoveredIndex]
                    const curScale = (iconData.scale !== undefined && iconData.scale > 1.0) ? iconData.scale : 1.35
                    const bounce   = (iconData.bounceOffset !== undefined) ? iconData.bounceOffset : 0.0
                    const sz       = dockConstants.baseSize * curScale
                    const computed = (rootWindow.pillY + 7.0) - (sz - dockConstants.baseSize) - (bounce * 40.0)
                    if (activeItem && typeof activeItem.y === "number" && activeItem.y > 0) {
                        return Math.min(computed, activeItem.y)
                    }
                    return computed
                }

                width:   tooltipMetrics.width + 24.0
                height:  28.0
                x:       tipCenterX - width / 2.0
                y:       iconTopY - height - 14.0
                visible: shouldShow

                Behavior on x { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                Behavior on y { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
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

                Image {
                    id: cloneRealIcon
                    anchors.centerIn: parent
                    width:  parent.width * 0.78
                    height: parent.height * 0.78
                    source: rootWindow.draggedIconUrl
                    sourceSize.width: 96
                    sourceSize.height: 96
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: rootWindow.draggedIconUrl.length > 0 && status === Image.Ready
                }

                Text {
                    anchors.centerIn: parent
                    visible: !cloneRealIcon.visible
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
