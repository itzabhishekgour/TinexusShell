// ============================================================================
// FileVectorIcon.qml — Crisp Resolution-Independent Vector File & Folder Icon
// ============================================================================
import QtQuick
import QtQuick.Shapes

Item {
    id: root
    property string iconKind: "file" // "folder" | "image" | "video" | "audio" | "pdf" | "code" | "archive" | "file"
    property bool isDir: false
    property real iconSize: 44.0

    width: iconSize
    height: iconSize

    readonly property string kind: isDir ? "folder" : iconKind

    // ── 1. Folder Icon (macOS Finder Style) ──────────────────────
    Item {
        anchors.centerIn: parent
        width: root.iconSize * 0.90
        height: root.iconSize * 0.70
        visible: root.kind === "folder"

        // Folder back tab
        Rectangle {
            x: 0; y: 0
            width: parent.width * 0.45
            height: parent.height * 0.35
            radius: 3
            color: "#2563EB"
        }

        // Folder front flap with subtle gradient
        Rectangle {
            x: 0
            y: parent.height * 0.18
            width: parent.width
            height: parent.height * 0.82
            radius: 4

            gradient: Gradient {
                GradientStop { position: 0.0; color: "#3B82F6" }
                GradientStop { position: 1.0; color: "#1D4ED8" }
            }

            border.color: Qt.rgba(1, 1, 1, 0.20)
            border.width: 1
        }
    }

    // ── 2. Document Base with Folded Corner (For non-folder items) ──
    Item {
        anchors.centerIn: parent
        width: root.iconSize * 0.70
        height: root.iconSize * 0.88
        visible: root.kind !== "folder"

        // Document body
        Rectangle {
            anchors.fill: parent
            radius: 4
            color: {
                if (root.kind === "pdf") return "#991B1B";
                if (root.kind === "image") return "#0F766E";
                if (root.kind === "video") return "#581C87";
                if (root.kind === "audio") return "#C2410C";
                if (root.kind === "code") return "#1E293B";
                if (root.kind === "archive") return "#854D0E";
                return "#334155";
            }
            border.color: Qt.rgba(1, 1, 1, 0.25)
            border.width: 1

            // Document fold corner in top right
            Shape {
                anchors.top: parent.top
                anchors.right: parent.right
                width: 10
                height: 10
                ShapePath {
                    fillColor: Qt.rgba(1, 1, 1, 0.25)
                    strokeColor: "transparent"
                    startX: 0; startY: 0
                    PathLine { x: 10; y: 10 }
                    PathLine { x: 0; y: 10 }
                    PathLine { x: 0; y: 0 }
                }
            }

            // Center Badge / Glyph
            Item {
                anchors.centerIn: parent
                width: parent.width * 0.65
                height: parent.height * 0.45

                // Code bracket `< / >`
                Text {
                    anchors.centerIn: parent
                    visible: root.kind === "code"
                    text: "</>"
                    color: "#38BDF8"
                    font.bold: true
                    font.pixelSize: Math.max(9, Math.round(root.iconSize * 0.22))
                }

                // PDF badge
                Text {
                    anchors.centerIn: parent
                    visible: root.kind === "pdf"
                    text: "PDF"
                    color: "#FFFFFF"
                    font.bold: true
                    font.pixelSize: Math.max(8, Math.round(root.iconSize * 0.18))
                }

                // Image symbol
                Text {
                    anchors.centerIn: parent
                    visible: root.kind === "image"
                    text: "IMG"
                    color: "#5EEAD4"
                    font.bold: true
                    font.pixelSize: Math.max(8, Math.round(root.iconSize * 0.18))
                }

                // Video symbol
                Text {
                    anchors.centerIn: parent
                    visible: root.kind === "video"
                    text: "▶"
                    color: "#E9D5FF"
                    font.pixelSize: Math.max(10, Math.round(root.iconSize * 0.24))
                }

                // Audio symbol
                Text {
                    anchors.centerIn: parent
                    visible: root.kind === "audio"
                    text: "♫"
                    color: "#FED7AA"
                    font.bold: true
                    font.pixelSize: Math.max(10, Math.round(root.iconSize * 0.24))
                }

                // Archive symbol
                Text {
                    anchors.centerIn: parent
                    visible: root.kind === "archive"
                    text: "ZIP"
                    color: "#FEF08A"
                    font.bold: true
                    font.pixelSize: Math.max(8, Math.round(root.iconSize * 0.18))
                }

                // Generic text document lines
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    visible: root.kind === "file"

                    Rectangle { width: 14; height: 1.5; color: Qt.rgba(1, 1, 1, 0.6); radius: 0.5 }
                    Rectangle { width: 10; height: 1.5; color: Qt.rgba(1, 1, 1, 0.6); radius: 0.5 }
                    Rectangle { width: 12; height: 1.5; color: Qt.rgba(1, 1, 1, 0.6); radius: 0.5 }
                }
            }
        }
    }
}
