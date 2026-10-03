// ============================================================================
// DockIcon.qml — Tinexus Dock: Icon Cell with Pure QML Fisheye Magnification
// Slice 4: Pure QML cosine curve math, SmoothedAnimation lag, LiquidGlass style.
// Ref: Architecture Blueprint §5.1, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
import QtQuick

Item {
    id: root

    // ── Public API ───────────────────────────────────────────────────────────
    property string appId:           ""
    property string label:           ""
    property string iconType:        "terminal" // "terminal"|"folder"|"gear"|"barchart"|"firefox"|"package"|"trash"
    property int    appState:        0          // 0=NotRunning 1=RunningBg 2=RunningFocused 3=Minimized
    property real   bounceOffset:    0.0
    property bool   isHovered:       false
    property bool   isRunning:       false
    property bool   isActive:        false
    property bool   needsAttention:  false
    property int    toplevelCount:   0
    property int    badgeCount:      0
    property bool   isDeleted:       false      // Show "?" badge

    // ── Slice 5: Drag & Rearrange State ──────────────────────────────────────
    property bool isBeingDragged: false
    property int  itemIndex:      -1
    property bool isDraggable:    !isDeleted && iconType !== "separator" && iconType !== "trash"

    // ── Slice 6: Wayland File Drag-and-Drop Drop Target ─────────────────────
    property bool isDropTarget:   false

    // ── Slice 4: Pure QML Fisheye Math Inputs ────────────────────────────────
    property real cursorX:       -1.0
    property bool mouseInDock:   false
    property real centerX:       0.0
    property real magnifySpread: 130.0
    property real maxScale:      1.35

    signal clicked()
    signal contextMenuRequested()
    signal dragStarted(int index, real globalX, real globalY, string appId)
    signal dragMoved(real globalX, real globalY)
    signal dragFinished(real globalX, real globalY)
    signal dragCanceled()

    opacity: isBeingDragged ? 0.20 : 1.0
    scale:   isBeingDragged ? 0.85 : (isDropTarget ? 1.24 : 1.0)
    Behavior on opacity { NumberAnimation { duration: 150 } }
    Behavior on scale {
        NumberAnimation {
            duration: 180
            easing.type: Easing.OutBack
            easing.overshoot: 1.4
        }
    }

    // ── 1. Pure QML Fisheye Math (Cosine curve, zero timers) ─────────────────
    readonly property real distanceToCursor: mouseInDock ? Math.abs(centerX - cursorX) : 999999.0

    readonly property real targetScale: {
        if (!mouseInDock || distanceToCursor >= magnifySpread) return 1.0
        // Normalized distance t in [0.0, 1.0] where 1.0 is directly under cursor
        const t = 1.0 - (distanceToCursor / magnifySpread)
        // Cosine bell curve: cos((1 - t) * PI * 0.5)
        return 1.0 + (maxScale - 1.0) * Math.cos((1.0 - t) * Math.PI * 0.5)
    }

    // SmoothedAnimation applies natural physics-based lag behind cursor
    property real scaleFactor: targetScale
    Behavior on scaleFactor {
        SmoothedAnimation {
            velocity: 8.0
            duration: 100
            reversingMode: SmoothedAnimation.Immediate
        }
    }

    // ── Derived geometry ─────────────────────────────────────────────────────
    property real baseIconSize:          48.0
    readonly property real iconSize:     baseIconSize * scaleFactor
    readonly property real cornerRadius: Math.round(11.5 * scaleFactor)

    width:  iconSize
    height: iconSize

    // ────────────────────────────────────────────────────────────────────────
    // 2. Drop Target Aura (Slice 6) & Hover Aura
    // ────────────────────────────────────────────────────────────────────────
    Rectangle {
        id: dropTargetAura
        anchors.centerIn: parent
        width:   iconSize + 16
        height:  iconSize + 16
        radius:  root.cornerRadius + 8
        color:   Qt.rgba(0.22, 0.71, 1.0, 0.18)
        border.color: "#38B6FF"
        border.width: 3
        visible: root.isDropTarget && root.iconType !== "separator"
        opacity: root.isDropTarget ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: 160 } }
    }

    Rectangle {
        id: hoverAura
        anchors.centerIn: parent
        width:   iconSize + 10
        height:  iconSize + 10
        radius:  root.cornerRadius + 5
        color:   "transparent"
        border.color: Qt.rgba(1.0, 1.0, 1.0, isHovered ? 0.28 : (isActive ? 0.20 : 0.0))
        border.width: 2
        visible: root.iconType !== "separator"

        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    // Active ring (colored accent for focused app)
    Rectangle {
        anchors.centerIn: parent
        width:   iconSize + 6
        height:  iconSize + 6
        radius:  root.cornerRadius + 3
        color:   "transparent"
        border.color: Qt.rgba(0.36, 0.72, 1.0, isActive ? 0.45 : 0.0) // Tinexus accent blue
        border.width: 2
        visible: isActive && root.iconType !== "separator"

        Behavior on border.color { ColorAnimation { duration: 150 } }
    }

    // Attention aura (pulsing amber glow when needsAttention is true)
    Rectangle {
        id: attentionAura
        anchors.centerIn: parent
        width:   iconSize + 8
        height:  iconSize + 8
        radius:  root.cornerRadius + 4
        color:   "transparent"
        border.color: Qt.rgba(1.0, 0.72, 0.12, root.needsAttention ? 0.90 : 0.0)
        border.width: 2
        visible: root.needsAttention && root.iconType !== "separator"

        SequentialAnimation on opacity {
            running: root.needsAttention
            loops: Animation.Infinite
            NumberAnimation { from: 0.35; to: 1.0; duration: 550; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 1.0; to: 0.35; duration: 550; easing.type: Easing.InOutQuad }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 3. Icon Background — LiquidGlass material per app type
    // ────────────────────────────────────────────────────────────────────────
    Rectangle {
        id: iconBg
        anchors.centerIn: parent
        width:  iconSize
        height: iconSize
        radius: root.cornerRadius
        clip:   true
        visible: root.iconType !== "separator"

        // Per-app color palette
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop {
                position: 0.0
                color: {
                    switch (root.iconType) {
                    case "folder":   return "#2B7DE9"
                    case "stack":    return "#00B4D8"
                    case "gear":     return "#6B6B70"
                    case "barchart": return "#0E1520"
                    case "firefox":  return "#E8450A"
                    case "package":  return "#0A7DFF"
                    case "trash":    return "#3A3A3C"
                    default:         return "#131625" // terminal
                    }
                }
            }
            GradientStop {
                position: 1.0
                color: {
                    switch (root.iconType) {
                    case "folder":   return "#0048B8"
                    case "stack":    return "#0077B6"
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

        // Glass border with Cyan highlight on Drop Target
        border.color: root.isDropTarget ? "#38B6FF" : Qt.rgba(1.0, 1.0, 1.0, 0.18)
        border.width: root.isDropTarget ? 2 : 1
        Behavior on border.color { ColorAnimation { duration: 140 } }

        // Inner top specular gloss
        Rectangle {
            x: 1; y: 1
            width:  parent.width - 2
            height: parent.height * 0.45
            radius: root.cornerRadius - 1
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: Qt.rgba(1.0, 1.0, 1.0, 0.14) }
                GradientStop { position: 1.0; color: Qt.rgba(1.0, 1.0, 1.0, 0.00) }
            }
        }

        // ── Glyphs ──────────────────────────────────────────────────────────

        // 1. Terminal: ">_" prompt
        Text {
            anchors.centerIn: parent
            visible:     root.iconType === "terminal"
            text:        ">_"
            color:       "#2ECC6A"
            font.family: "JetBrains Mono, Fira Mono, monospace"
            font.bold:   true
            font.pixelSize: Math.max(8, Math.round(18 * root.scaleFactor))
        }

        // 2. Folder: tab + body
        Item {
            anchors.centerIn: parent
            width:   26 * root.scaleFactor
            height:  20 * root.scaleFactor
            visible: root.iconType === "folder"

            Rectangle {
                x: 0; y: 0
                width:  parent.width * 0.42
                height: 5 * root.scaleFactor
                radius: 2 * root.scaleFactor
                color:  Qt.rgba(1.0, 1.0, 1.0, 0.92)
            }
            Rectangle {
                x: 0; y: 3.5 * root.scaleFactor
                width:  parent.width
                height: parent.height - 3.5 * root.scaleFactor
                radius: 3 * root.scaleFactor
                color:  Qt.rgba(1.0, 1.0, 1.0, 0.92)
            }
        }

        // 3. Settings: ⚙ gear
        Text {
            anchors.centerIn: parent
            visible:        root.iconType === "gear"
            text:           "⚙"
            color:          "#FFFFFF"
            font.pixelSize: Math.max(8, Math.round(22 * root.scaleFactor))
        }

        // 4. Activity Monitor: heartbeat pulse
        Canvas {
            anchors.fill: parent
            visible: root.iconType === "barchart"
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                var s = width / 40.0
                ctx.strokeStyle = "#2ECC6A"
                ctx.lineWidth   = 2.2 * s
                ctx.lineCap     = "round"
                ctx.lineJoin    = "round"
                ctx.beginPath()
                ctx.moveTo(5 * s, 20 * s)
                ctx.lineTo(12 * s, 20 * s)
                ctx.lineTo(16 * s, 10 * s)
                ctx.lineTo(20 * s, 30 * s)
                ctx.lineTo(24 * s, 14 * s)
                ctx.lineTo(28 * s, 20 * s)
                ctx.lineTo(35 * s, 20 * s)
                ctx.stroke()
            }
        }

        // 5. App Store: bold "A"
        Text {
            anchors.centerIn: parent
            visible:        root.iconType === "package"
            text:           "A"
            color:          "#FFFFFF"
            font.bold:      true
            font.pixelSize: Math.max(8, Math.round(24 * root.scaleFactor))
        }

        // 6. Firefox: globe + flame
        Canvas {
            anchors.fill: parent
            visible: root.iconType === "firefox"
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                var s = width / 24.0

                // Globe (blue)
                ctx.fillStyle = "#1E6FEB"
                ctx.beginPath()
                ctx.arc(12 * s, 12 * s, 7.5 * s, 0, 2 * Math.PI)
                ctx.fill()

                // Latitude arc (white, subtle)
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.35)
                ctx.lineWidth   = 0.9 * s
                ctx.beginPath()
                ctx.arc(12 * s, 12 * s, 7.5 * s, 0, Math.PI)
                ctx.stroke()

                // Flame wrap (orange)
                ctx.strokeStyle = "#FF9500"
                ctx.lineWidth   = 2.8 * s
                ctx.lineCap     = "round"
                ctx.beginPath()
                ctx.arc(12 * s, 12 * s, 9.5 * s, -0.55 * Math.PI, 0.75 * Math.PI, false)
                ctx.stroke()

                // Flame tip (yellow)
                ctx.fillStyle = "#FFD60A"
                ctx.beginPath()
                ctx.arc(14 * s, 3.2 * s, 2.2 * s, 0, 2 * Math.PI)
                ctx.fill()
            }
        }

        // 7. Trash can
        Canvas {
            anchors.fill: parent
            visible: root.iconType === "trash"
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                var s = width / 40.0
                ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.75)
                ctx.lineWidth   = 2.0 * s
                ctx.lineCap     = "round"
                ctx.lineJoin    = "round"

                // Lid
                ctx.beginPath()
                ctx.moveTo(10 * s, 13 * s)
                ctx.lineTo(30 * s, 13 * s)
                ctx.stroke()

                // Handle
                ctx.beginPath()
                ctx.moveTo(16 * s, 13 * s)
                ctx.lineTo(16 * s, 10 * s)
                ctx.lineTo(24 * s, 10 * s)
                ctx.lineTo(24 * s, 13 * s)
                ctx.stroke()

                // Body
                ctx.beginPath()
                ctx.moveTo(12 * s, 14 * s)
                ctx.lineTo(13 * s, 31 * s)
                ctx.lineTo(27 * s, 31 * s)
                ctx.lineTo(28 * s, 14 * s)
                ctx.stroke()

                // Vertical lines inside body
                ctx.beginPath()
                ctx.moveTo(20 * s, 17 * s)
                ctx.lineTo(20 * s, 28 * s)
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(16 * s, 17 * s)
                ctx.lineTo(16.5 * s, 28 * s)
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(24 * s, 17 * s)
                ctx.lineTo(23.5 * s, 28 * s)
                ctx.stroke()
            }
        }

        // 8. Stack glyph: cascading layered rectangles / folder stack
        Item {
            anchors.centerIn: parent
            width:  32 * root.scaleFactor
            height: 32 * root.scaleFactor
            visible: root.iconType === "stack"

            Rectangle {
                x: parent.width * 0.22; y: parent.height * 0.16
                width: parent.width * 0.56; height: parent.height * 0.52
                radius: 4 * root.scaleFactor
                color: Qt.rgba(1, 1, 1, 0.25)
                border.color: Qt.rgba(1, 1, 1, 0.45)
                border.width: 1
            }
            Rectangle {
                x: parent.width * 0.16; y: parent.height * 0.26
                width: parent.width * 0.68; height: parent.height * 0.54
                radius: 4 * root.scaleFactor
                color: Qt.rgba(1, 1, 1, 0.40)
                border.color: Qt.rgba(1, 1, 1, 0.60)
                border.width: 1
            }
            Rectangle {
                x: parent.width * 0.10; y: parent.height * 0.38
                width: parent.width * 0.80; height: parent.height * 0.54
                radius: 5 * root.scaleFactor
                color: Qt.rgba(1, 1, 1, 0.70)
                border.color: "#FFFFFF"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "↓"
                    font.pixelSize: Math.round(13 * root.scaleFactor)
                    font.bold: true
                    color: "#0048B8"
                }
            }
        }

        // Deleted app badge: "?"
        Rectangle {
            anchors.fill: parent
            visible:      root.isDeleted
            color:        Qt.rgba(0.1, 0.1, 0.16, 0.7)
            radius:       root.cornerRadius

            Text {
                anchors.centerIn: parent
                text:           "?"
                color:          Qt.rgba(1, 1, 1, 0.8)
                font.bold:      true
                font.pixelSize: Math.max(8, Math.round(20 * root.scaleFactor))
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 4. Running Indicator Dots — below icon
    // ────────────────────────────────────────────────────────────────────────
    Row {
        id: dotRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top:              parent.bottom
        anchors.topMargin:        5
        spacing:                  4
        visible:                  root.isRunning

        Repeater {
            model: root.isRunning ? Math.min(3, Math.max(1, root.toplevelCount)) : 0

            Item {
                width:  10
                height: 10

                // Soft halo (focused only)
                Rectangle {
                    anchors.centerIn: parent
                    width:   10
                    height:  10
                    radius:  5
                    color:   Qt.rgba(1, 1, 1, root.isActive ? 0.22 : 0.0)
                    visible: root.isActive

                    Behavior on color { ColorAnimation { duration: 180 } }
                }

                // Core dot
                Rectangle {
                    anchors.centerIn: parent
                    width:   root.isActive ? 5 : 4
                    height:  width
                    radius:  width / 2.0
                    color:   root.isActive ? "#FFFFFF"
                             : (root.appState === 3 ? Qt.rgba(1,1,1,0.35) : Qt.rgba(1,1,1,0.60))

                    Behavior on width { NumberAnimation { duration: 150 } }
                    Behavior on color { ColorAnimation  { duration: 180 } }
                }
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 5. Notification Badge — red pill top-right
    // ────────────────────────────────────────────────────────────────────────
    Item {
        id: badgePill
        anchors.top:   parent.top
        anchors.right: parent.right
        anchors.topMargin:   -4
        anchors.rightMargin: -4
        visible:             root.badgeCount > 0

        readonly property int count:         root.badgeCount
        readonly property bool isLargeCount: count > 9

        width:  isLargeCount ? (badgeText.implicitWidth + 10) : 18
        height: 18

        scale: root.badgeCount > 0 ? 1.0 : 0.0
        opacity: root.badgeCount > 0 ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 100 } }

        SequentialAnimation on scale {
            id: badgePopAnim
            running: false
            NumberAnimation { from: 0.2; to: 1.2; duration: 130; easing.type: Easing.OutBack; easing.overshoot: 1.4 }
            NumberAnimation { to: 1.0; duration: 90; easing.type: Easing.OutQuad }
        }

        onCountChanged: {
            if (root.badgeCount > 0) {
                badgePopAnim.restart()
            }
        }

        Rectangle {
            anchors.fill: parent
            radius:       9
            color:        "#FF3B30"
            border.color: Qt.rgba(0, 0, 0, 0.30)
            border.width: 1

            Text {
                id:             badgeText
                anchors.centerIn: parent
                text:           root.badgeCount > 99 ? "99+" : root.badgeCount.toString()
                color:          "#FFFFFF"
                font.pixelSize: 10
                font.bold:      true
                font.family:    "Inter, SF Pro Text, sans-serif"
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 6. Interaction MouseArea (Click, ContextMenu & Slice 5 Drag)
    // ────────────────────────────────────────────────────────────────────────
    MouseArea {
        id: mouseArea
        anchors.fill:    parent
        enabled:         root.iconType !== "separator"
        cursorShape:     dragActive ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        z:               20
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property point pressOrigin: Qt.point(0, 0)
        property bool  dragActive:  false

        onPressed: function(mouse) {
            pressOrigin = Qt.point(mouse.x, mouse.y)
            dragActive = false
        }

        onPositionChanged: function(mouse) {
            if (mouse.buttons & Qt.LeftButton) {
                if (!dragActive && root.isDraggable) {
                    const dx = mouse.x - pressOrigin.x
                    const dy = mouse.y - pressOrigin.y
                    if (Math.abs(dx) > 8 || Math.abs(dy) > 8) {
                        dragActive = true
                        root.isBeingDragged = true
                        const pt = root.mapToItem(null, mouse.x, mouse.y)
                        root.dragStarted(root.itemIndex, pt.x, pt.y, root.appId)
                    }
                }
                if (dragActive) {
                    const pt = root.mapToItem(null, mouse.x, mouse.y)
                    root.dragMoved(pt.x, pt.y)
                }
            }
        }

        onReleased: function(mouse) {
            if (dragActive) {
                dragActive = false
                root.isBeingDragged = false
                const pt = root.mapToItem(null, mouse.x, mouse.y)
                root.dragFinished(pt.x, pt.y)
            } else if (mouse.button === Qt.RightButton || (mouse.modifiers & Qt.ControlModifier)) {
                root.contextMenuRequested()
            } else if (mouse.button === Qt.LeftButton) {
                root.clicked()
            }
        }

        onCanceled: {
            if (dragActive) {
                dragActive = false
                root.isBeingDragged = false
                root.dragCanceled()
            }
        }

        onPressAndHold: function(mouse) {
            if (!dragActive) {
                root.contextMenuRequested()
            }
        }
    }
}
