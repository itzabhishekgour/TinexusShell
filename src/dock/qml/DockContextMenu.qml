// ============================================================================
// DockContextMenu.qml — Tinexus Dock: LiquidGlass Context Menu Overlay
// Slice 3: Right-Click / Control-Click Menu on LayerOverlay
// Ref: Architecture Blueprint §1.1, Decision B, docs/05_UI_UX_GUIDELINES.md
// ============================================================================
import QtQuick
import QtQuick.Window

Window {
    id: rootWindow

    width:  typeof menuPopup !== "undefined" ? menuPopup.screenWidth  : (Screen.width > 0 ? Screen.width : 1920)
    height: typeof menuPopup !== "undefined" ? menuPopup.screenHeight : (Screen.height > 0 ? Screen.height : 1080)
    color:  "transparent"
    flags:  Qt.Popup | Qt.FramelessWindowHint
    visible: typeof menuPopup !== "undefined" ? menuPopup.isOpen : false

    // Update screen geometry if resized
    onWidthChanged:  if (typeof menuPopup !== "undefined") menuPopup.updateScreenGeometry(width, height)
    onHeightChanged: if (typeof menuPopup !== "undefined") menuPopup.updateScreenGeometry(width, height)

    // Keyboard dismiss on Escape
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (typeof menuPopup !== "undefined") menuPopup.hideMenu()
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1. Outside-click dismiss catcher covering entire screen
    // ────────────────────────────────────────────────────────────────────────
    MouseArea {
        id: dismissArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        onPressed: {
            if (typeof menuPopup !== "undefined") menuPopup.hideMenu()
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 2. LiquidGlass Context Menu Card
    // ────────────────────────────────────────────────────────────────────────
    Item {
        id: menuCard
        z: 10

        readonly property real cardX: typeof menuPopup !== "undefined" ? menuPopup.menuX : 100.0
        readonly property real cardY: typeof menuPopup !== "undefined" ? menuPopup.menuY : 100.0
        readonly property real cardW: typeof menuPopup !== "undefined" ? menuPopup.menuWidth : 220.0
        readonly property real cardH: contentCol.implicitHeight + 16.0

        x: cardX
        y: cardY
        width: cardW
        height: cardH

        // Prevent outside-click dismiss when clicking inside the card
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onPressed: {}
        }

        // Bloom entry animation
        transformOrigin: Item.Bottom
        scale: rootWindow.visible ? 1.0 : 0.94
        opacity: rootWindow.visible ? 1.0 : 0.0

        Behavior on scale {
            NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
        }
        Behavior on opacity {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }

        // ── Drop Shadows ────────────────────────────────────────────────────
        Rectangle {
            x: -4; y: 6
            width: parent.width + 8
            height: parent.height
            radius: 16
            color: Qt.rgba(0.0, 0.0, 0.0, 0.32)
        }
        Rectangle {
            x: -2; y: 3
            width: parent.width + 4
            height: parent.height
            radius: 15
            color: Qt.rgba(0.0, 0.0, 0.0, 0.20)
        }

        // ── LiquidGlass Card Body ───────────────────────────────────────────
        Rectangle {
            id: cardBg
            anchors.fill: parent
            radius: 14
            clip: false

            // Tri-stop LiquidGlass dark frosted gradient
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.00; color: Qt.rgba(0.085, 0.090, 0.140, 0.94) }
                GradientStop { position: 0.45; color: Qt.rgba(0.065, 0.068, 0.110, 0.96) }
                GradientStop { position: 1.00; color: Qt.rgba(0.040, 0.042, 0.075, 0.98) }
            }

            // Outer glass border
            border.color: Qt.rgba(1.0, 1.0, 1.0, 0.18)
            border.width: 1
        }

        // Top specular highlight rim (11% shimmer)
        Rectangle {
            x: 1; y: 1
            width: parent.width - 2
            height: 36
            radius: 13
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: Qt.rgba(1.0, 1.0, 1.0, 0.11) }
                GradientStop { position: 1.0; color: Qt.rgba(1.0, 1.0, 1.0, 0.00) }
            }
        }

        // Inner bottom edge glow
        Rectangle {
            x: 2
            y: parent.height - 3
            width: parent.width - 4
            height: 3
            radius: 2
            color: Qt.rgba(1.0, 1.0, 1.0, 0.06)
        }

        // ── Content Column ──────────────────────────────────────────────────
        Column {
            id: contentCol
            x: 6
            y: 8
            width: parent.width - 12
            spacing: 2

            // ── Section 1: Header (App Name & Status) ───────────────────────
            Item {
                width: parent.width
                height: 32

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8
                    Item {
                        width: 16; height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            anchors.centerIn: parent
                            width: 8; height: 8
                            radius: 4
                            color: {
                                if (typeof menuPopup === "undefined") return "#8E8E93"
                                if (menuPopup.isActive) return "#30D158"
                                if (menuPopup.isRunning) return "#0A84FF"
                                return "#8E8E93"
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: typeof menuPopup !== "undefined" ? menuPopup.targetDisplayName : ""
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        font.family: "Inter, SF Pro Display, sans-serif"
                        elide: Text.ElideRight
                        width: parent.width - 28
                    }
                }
            }

            // Divider
            Rectangle {
                width: parent.width - 8
                x: 4
                height: 1
                color: Qt.rgba(1.0, 1.0, 1.0, 0.12)
            }

            // ── Section 2: Windows List (if multiple windows running) ────────
            Repeater {
                model: (typeof menuPopup !== "undefined" && menuPopup.isRunning) ? menuPopup.windowsList : []
                delegate: Item {
                    width: contentCol.width
                    height: 28

                    Rectangle {
                        id: winRowBg
                        anchors.fill: parent
                        radius: 6
                        color: winHover.containsMouse ? Qt.rgba(0.24, 0.52, 0.95, 0.88) : "transparent"
                    }

                    MouseArea {
                        id: winHover
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof menuPopup !== "undefined") {
                                menuPopup.triggerAction("activate_window", { handle: modelData.handle })
                            }
                        }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 8

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.isActivated ? "●" : "○"
                            color: winHover.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.60)
                            font.pixelSize: 10
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.title
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.family: "Inter, SF Pro Text, sans-serif"
                            elide: Text.ElideRight
                            width: parent.width - 24
                        }
                    }
                }
            }

            // Show All Windows / Hide (if running)
            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && menuPopup.isRunning

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: showAllHover.containsMouse ? Qt.rgba(0.24, 0.52, 0.95, 0.88) : "transparent"
                }
                MouseArea {
                    id: showAllHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("show_all")
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Show All Windows"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.family: "Inter, SF Pro Text, sans-serif"
                }
            }

            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && menuPopup.isRunning

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: hideHover.containsMouse ? Qt.rgba(0.24, 0.52, 0.95, 0.88) : "transparent"
                }
                MouseArea {
                    id: hideHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("hide_all")
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Hide"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.family: "Inter, SF Pro Text, sans-serif"
                }
            }

            // Divider after windows
            Rectangle {
                width: parent.width - 8
                x: 4
                height: 1
                color: Qt.rgba(1.0, 1.0, 1.0, 0.12)
                visible: typeof menuPopup !== "undefined" && menuPopup.isRunning
            }

            // ── Section 3: Open (if not running and not trash) ───────────────
            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && !menuPopup.isRunning && menuPopup.targetKind !== 6

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: openHover.containsMouse ? Qt.rgba(0.24, 0.52, 0.95, 0.88) : "transparent"
                }
                MouseArea {
                    id: openHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("open")
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Open"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    font.family: "Inter, SF Pro Text, sans-serif"
                }
            }

            // ── Section 4: Keep in Dock toggle ──────────────────────────────
            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && menuPopup.targetKind !== 6 && menuPopup.targetKind !== 3

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: keepHover.containsMouse ? Qt.rgba(0.24, 0.52, 0.95, 0.88) : "transparent"
                }
                MouseArea {
                    id: keepHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("toggle_keep_in_dock")
                    }
                }
                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Keep in Dock"
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.family: "Inter, SF Pro Text, sans-serif"
                        width: parent.width - 20
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: (typeof menuPopup !== "undefined" && menuPopup.isPinned) ? "✓" : ""
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        font.bold: true
                    }
                }
            }

            // Remove from Dock (if pinned and not running)
            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && menuPopup.isPinned && !menuPopup.isRunning && menuPopup.targetKind !== 6

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: removeHover.containsMouse ? Qt.rgba(0.24, 0.52, 0.95, 0.88) : "transparent"
                }
                MouseArea {
                    id: removeHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("remove_from_dock")
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Remove from Dock"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.family: "Inter, SF Pro Text, sans-serif"
                }
            }

            // ── Section 5: Trash Specific Actions ───────────────────────────
            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && menuPopup.targetKind === 6

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: openTrashHover.containsMouse ? Qt.rgba(0.24, 0.52, 0.95, 0.88) : "transparent"
                }
                MouseArea {
                    id: openTrashHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("open_trash")
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Open Trash"
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.family: "Inter, SF Pro Text, sans-serif"
                }
            }

            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && menuPopup.targetKind === 6

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: emptyTrashHover.containsMouse ? Qt.rgba(0.85, 0.20, 0.20, 0.80) : "transparent"
                }
                MouseArea {
                    id: emptyTrashHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("empty_trash")
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Empty Trash"
                    color: emptyTrashHover.containsMouse ? "#FFFFFF" : "#FF453A"
                    font.pixelSize: 12
                    font.family: "Inter, SF Pro Text, sans-serif"
                }
            }

            // Divider before Quit
            Rectangle {
                width: parent.width - 8
                x: 4
                height: 1
                color: Qt.rgba(1.0, 1.0, 1.0, 0.12)
                visible: typeof menuPopup !== "undefined" && menuPopup.isRunning
            }

            // ── Section 6: Quit / Force Quit (if running) ───────────────────
            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && menuPopup.isRunning

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: quitHover.containsMouse ? Qt.rgba(0.85, 0.20, 0.20, 0.80) : "transparent"
                }
                MouseArea {
                    id: quitHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("quit")
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Quit"
                    color: quitHover.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.92)
                    font.pixelSize: 12
                    font.family: "Inter, SF Pro Text, sans-serif"
                }
            }

            Item {
                width: contentCol.width
                height: 28
                visible: typeof menuPopup !== "undefined" && menuPopup.isRunning

                Rectangle {
                    anchors.fill: parent
                    radius: 6
                    color: forceQuitHover.containsMouse ? Qt.rgba(0.85, 0.20, 0.20, 0.90) : "transparent"
                }
                MouseArea {
                    id: forceQuitHover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (typeof menuPopup !== "undefined") menuPopup.triggerAction("force_quit")
                    }
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Force Quit"
                    color: forceQuitHover.containsMouse ? "#FFFFFF" : Qt.rgba(1, 0.4, 0.4, 0.85)
                    font.pixelSize: 12
                    font.family: "Inter, SF Pro Text, sans-serif"
                }
            }
        }
    }
}
