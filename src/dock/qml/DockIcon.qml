// ============================================================================
// DockIcon.qml — Tinexus Dock: Premium Icon Cell (LiquidGlass Style)
// Renders: icon background + glyph, running indicator dots, notification badge,
//          hover aura, and bounce offset. Used by DockBar.qml Repeater.
// Ref: Architecture Blueprint §5.3-5.4, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    // ── Public API ───────────────────────────────────────────────────────────
    property string appId:       ""
    property string label:       ""
    property string iconType:    "terminal"    // "terminal"|"folder"|"gear"|"barchart"|"firefox"|"package"|"trash"
    property int    appState:    0             // 0=NotRunning 1=RunningBg 2=RunningFocused 3=Minimized
    property real   scaleFactor: 1.0
    property real   bounceOffset: 0.0
    property bool   isHovered:   false
    property bool   isRunning:   false
    property bool   isActive:    false
    property int    badgeCount:  0
    property bool   isDeleted:   false         // Show "?" badge

    signal clicked()

    // ── Derived geometry ─────────────────────────────────────────────────────
    readonly property real baseIconSize: 48.0
    readonly property real iconSize:     baseIconSize * scaleFactor
    readonly property real cornerRadius: 11.0 * scaleFactor

    width:  iconSize
    height: iconSize

    // ────────────────────────────────────────────────────────────────────────
    // 1. Hover Aura — soft glow ring around focused/hovered icon
    // ────────────────────────────────────────────────────────────────────────
    Rectangle {
        id: hoverAura
        anchors.centerIn: parent
        width:   iconSize + 10
        height:  iconSize + 10
        radius:  root.cornerRadius + 5
        color:   "transparent"
        border.color: Qt.rgba(1.0, 1.0, 1.0, isHovered ? 0.28 : (isActive ? 0.20 : 0.0))
        border.width: 2

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
        visible: isActive

        Behavior on border.color { ColorAnimation { duration: 150 } }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 2. Icon Background — LiquidGlass material per app type
    // ────────────────────────────────────────────────────────────────────────
    Rectangle {
        id: iconBg
        anchors.centerIn: parent
        width:  iconSize
        height: iconSize
        radius: root.cornerRadius
        clip:   true

        // Per-app color palette
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop {
                position: 0.0
                color: {
                    switch (root.iconType) {
                    case "folder":   return "#2B7DE9"
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

        // Glass border
        border.color: Qt.rgba(1.0, 1.0, 1.0, 0.18)
        border.width: 1

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

        // ── Glyphs (only one visible at a time) ─────────────────────────────

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
    // 3. Running Indicator Dots — below icon, visible when app is running
    // ────────────────────────────────────────────────────────────────────────
    Row {
        id: dotRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top:              parent.bottom
        anchors.topMargin:        5
        spacing:                  4
        visible:                  root.isRunning

        Repeater {
            model: root.isRunning ? 1 : 0  // Single dot for Slice 1; Slice 2 maps toplevelCount

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
    // 4. Notification Badge — red pill top-right
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

        // Scale-pop entry animation on appearance
        scale: root.badgeCount > 0 ? 1.0 : 0.0
        Behavior on scale {
            SequentialAnimation {
                NumberAnimation { to: 1.25; duration: 100; easing.type: Easing.OutCubic }
                NumberAnimation { to: 1.0;  duration: 80;  easing.type: Easing.InCubic  }
            }
        }

        Rectangle {
            anchors.fill: parent
            radius:       9
            color:        "#FF3B30"  // iOS-style red
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
    // 5. Click handler
    // ────────────────────────────────────────────────────────────────────────
    MouseArea {
        anchors.fill:    parent
        cursorShape:     Qt.PointingHandCursor
        z:               20
        onClicked:       root.clicked()
        onPressAndHold:  root.clicked()  // future: context menu trigger
    }
}
