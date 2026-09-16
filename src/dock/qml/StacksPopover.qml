// ============================================================================
// StacksPopover.qml — LiquidGlass Folder Aggregator Popover (Slice 7)
// Ref: Architecture Blueprint §8, docs/05_UI_UX_GUIDELINES.md
// Grid view layout with scale entry animation, inotify model, and file opening.
// ============================================================================
import QtQuick
import QtQuick.Controls

Window {
    id: rootWindow

    width:  typeof stacksPopup !== "undefined" ? stacksPopup.screenWidth  : 1920
    height: typeof stacksPopup !== "undefined" ? stacksPopup.screenHeight : 1080
    color:  "transparent"
    flags:  Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    // ── Dismissal on Escape key ──────────────────────────────────────────────
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (typeof stacksPopup !== "undefined") stacksPopup.hidePopup()
        }
    }

    // ── Fullscreen Outside-Click Dismissal ───────────────────────────────────
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: function(mouse) {
            const px = popoverPill.x
            const py = popoverPill.y
            const pw = popoverPill.width
            const ph = popoverPill.height

            // If click is outside the popover card, dismiss it
            if (mouse.x < px || mouse.x > px + pw || mouse.y < py || mouse.y > py + ph) {
                if (typeof stacksPopup !== "undefined") stacksPopup.hidePopup()
            }
        }
    }

    // ────────────────────────────────────────────────────────────────────────
    // 1. The LiquidGlass Popover Card
    // ────────────────────────────────────────────────────────────────────────
    Rectangle {
        id: popoverPill

        readonly property bool isOpen: typeof stacksPopup !== "undefined" && stacksPopup.isOpen

        x: typeof stacksPopup !== "undefined" ? stacksPopup.popupX : 200
        y: typeof stacksPopup !== "undefined" ? stacksPopup.popupY : 200
        width:  typeof stacksPopup !== "undefined" ? stacksPopup.popupWidth  : 440
        height: typeof stacksPopup !== "undefined" ? stacksPopup.popupHeight : 460
        radius: 20
        clip:   true

        // Scale & Opacity Entry Spring Animation
        scale:   isOpen ? 1.0 : 0.85
        opacity: isOpen ? 1.0 : 0.0
        Behavior on scale {
            NumberAnimation {
                duration: 200
                easing.type: Easing.OutBack
                easing.overshoot: 1.3
            }
        }
        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }

        // Tri-stop LiquidGlass dark background
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.00; color: Qt.rgba(0.09, 0.12, 0.18, 0.94) }
            GradientStop { position: 0.50; color: Qt.rgba(0.06, 0.08, 0.13, 0.96) }
            GradientStop { position: 1.00; color: Qt.rgba(0.04, 0.05, 0.09, 0.97) }
        }

        // 11% white specular border & subtle rim
        border.color: Qt.rgba(1.0, 1.0, 1.0, 0.15)
        border.width: 1

        // Top specular gloss line
        Rectangle {
            x: 2; y: 1
            width:  parent.width - 4
            height: 1
            color:  Qt.rgba(1.0, 1.0, 1.0, 0.28)
        }

        // Stop clicks inside popover card from bubbling to dismissal area
        MouseArea {
            anchors.fill: parent
            preventStealing: true
            onClicked: {}
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            // ────────────────────────────────────────────────────────────────
            // Header: Title, Count, Sort Mode Toggle, and Open Folder Button
            // ────────────────────────────────────────────────────────────────
            Item {
                width: parent.width
                height: 34

                // Left: Folder Glyph + Title / Count
                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    Rectangle {
                        width: 28; height: 28
                        radius: 7
                        anchors.verticalCenter: parent.verticalCenter
                        gradient: Gradient {
                            orientation: Gradient.Vertical
                            GradientStop { position: 0.0; color: "#2B7DE9" }
                            GradientStop { position: 1.0; color: "#0048B8" }
                        }
                        Text {
                            anchors.centerIn: parent
                            text: "📁"
                            font.pixelSize: 14
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 1

                        Text {
                            text: typeof stacksPopup !== "undefined" ? stacksPopup.folderTitle : "Downloads"
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.bold: true
                            font.family: "Inter, SF Pro Display, sans-serif"
                        }

                        Text {
                            text: (typeof stacksModel !== "undefined" ? stacksModel.fileCount : 0) + " items"
                            color: Qt.rgba(1.0, 1.0, 1.0, 0.50)
                            font.pixelSize: 11
                            font.family: "Inter, SF Pro Text, sans-serif"
                        }
                    }
                }

                // Right: Sort Mode Button + Open in Files Button
                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    // Sort Mode Toggle Button
                    Rectangle {
                        width: 80; height: 26
                        radius: 6
                        anchors.verticalCenter: parent.verticalCenter
                        color: sortMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.07)
                        border.color: Qt.rgba(1, 1, 1, 0.12)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: {
                                if (typeof stacksModel === "undefined") return "Recent"
                                switch (stacksModel.sortMode) {
                                case 0: return "↓ Date"
                                case 1: return "↑ Date"
                                case 2: return "A→Z"
                                case 3: return "Z→A"
                                case 4: return "Kind"
                                default: return "Sort"
                                }
                            }
                            color: Qt.rgba(1, 1, 1, 0.85)
                            font.pixelSize: 11
                            font.family: "Inter, sans-serif"
                        }

                        MouseArea {
                            id: sortMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof stacksModel !== "undefined") {
                                    const nextMode = (stacksModel.sortMode + 1) % 5
                                    stacksModel.setSortMode(nextMode)
                                }
                            }
                        }
                    }

                    // "Open in Files" Action Pill
                    Rectangle {
                        width: 90; height: 26
                        radius: 6
                        anchors.verticalCenter: parent.verticalCenter
                        color: openFilesMouse.containsMouse ? "#38B6FF" : Qt.rgba(0.22, 0.71, 1.0, 0.18)
                        border.color: "#38B6FF"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Open in Files"
                            color: openFilesMouse.containsMouse ? "#000000" : "#38B6FF"
                            font.pixelSize: 11
                            font.bold: true
                            font.family: "Inter, sans-serif"
                        }

                        MouseArea {
                            id: openFilesMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof stacksModel !== "undefined") stacksModel.openFolder()
                                if (typeof stacksPopup !== "undefined") stacksPopup.hidePopup()
                            }
                        }
                    }
                }
            }

            // Divider line
            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(1.0, 1.0, 1.0, 0.10)
            }

            // ────────────────────────────────────────────────────────────────
            // Body: Grid View of Items
            // ────────────────────────────────────────────────────────────────
            Item {
                width: parent.width
                height: popoverPill.height - 84
                clip: true

                // Empty State
                Text {
                    anchors.centerIn: parent
                    visible: typeof stacksModel !== "undefined" && stacksModel.fileCount === 0
                    text: "No files found in folder"
                    color: Qt.rgba(1.0, 1.0, 1.0, 0.40)
                    font.pixelSize: 13
                    font.family: "Inter, sans-serif"
                }

                GridView {
                    id: fileGrid
                    anchors.fill: parent
                    cellWidth:  Math.floor(width / 4)
                    cellHeight: 96
                    clip: true
                    model: typeof stacksModel !== "undefined" ? stacksModel : null
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Item {
                        id: cellItem
                        width:  fileGrid.cellWidth
                        height: fileGrid.cellHeight

                        readonly property bool isHov: itemMouse.containsMouse

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 4
                            radius: 10
                            color: isHov ? Qt.rgba(1.0, 1.0, 1.0, 0.10) : "transparent"
                            border.color: isHov ? Qt.rgba(1.0, 1.0, 1.0, 0.18) : "transparent"
                            border.width: 1

                            scale: isHov ? 1.04 : 1.0
                            Behavior on scale {
                                NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
                            }

                            Column {
                                anchors.centerIn: parent
                                spacing: 5
                                width: parent.width - 12

                                // File Category Icon
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 38; height: 38
                                    radius: 9
                                    clip: true

                                    gradient: Gradient {
                                        orientation: Gradient.Vertical
                                        GradientStop {
                                            position: 0.0
                                            color: {
                                                switch (model.iconType) {
                                                case "folder":  return "#2B7DE9"
                                                case "pdf":     return "#E83A30"
                                                case "image":   return "#00B4D8"
                                                case "code":    return "#8338EC"
                                                case "audio":   return "#FF006E"
                                                case "video":   return "#FB5607"
                                                case "archive": return "#FFBE0B"
                                                case "text":    return "#3A86FF"
                                                default:        return "#4A5568"
                                                }
                                            }
                                        }
                                        GradientStop {
                                            position: 1.0
                                            color: {
                                                switch (model.iconType) {
                                                case "folder":  return "#0048B8"
                                                case "pdf":     return "#991008"
                                                case "image":   return "#0077B6"
                                                case "code":    return "#3A0CA3"
                                                case "audio":   return "#A00045"
                                                case "video":   return "#C0392B"
                                                case "archive": return "#D48B00"
                                                case "text":    return "#1D4ED8"
                                                default:        return "#2D3748"
                                                }
                                            }
                                        }
                                    }

                                    border.color: Qt.rgba(1, 1, 1, 0.20)
                                    border.width: 1

                                    Text {
                                        anchors.centerIn: parent
                                        text: {
                                            switch (model.iconType) {
                                            case "folder":  return "📁"
                                            case "pdf":     return "PDF"
                                            case "image":   return "🖼"
                                            case "code":    return "< / >"
                                            case "audio":   return "🎵"
                                            case "video":   return "🎬"
                                            case "archive": return "📦"
                                            case "text":    return "📄"
                                            default:        return "📎"
                                            }
                                        }
                                        color: "#FFFFFF"
                                        font.pixelSize: model.iconType === "pdf" || model.iconType === "code" ? 9 : 15
                                        font.bold: true
                                        font.family: "Inter, sans-serif"
                                    }
                                }

                                // File Name
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    text: model.fileName
                                    color: isHov ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.85)
                                    font.pixelSize: 10
                                    font.family: "Inter, SF Pro Text, sans-serif"
                                    elide: Text.ElideMiddle
                                    maximumLineCount: 1
                                }

                                // File Size
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: model.fileSize
                                    color: Qt.rgba(1, 1, 1, 0.45)
                                    font.pixelSize: 9
                                    font.family: "Inter, sans-serif"
                                }
                            }

                            MouseArea {
                                id: itemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof stacksModel !== "undefined") {
                                        stacksModel.openFile(index)
                                    }
                                    if (typeof stacksPopup !== "undefined") {
                                        stacksPopup.hidePopup()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
