// ============================================================================
// DockIcon.qml — High-precision vector dock icon with running indicators
// ============================================================================
import QtQuick
import QtQuick.Shapes

Item {
    id: root

    property string appId: ""
    property string label: ""
    property string iconType: "terminal"
    property int appState: 0 // 0=NotRunning, 1=RunningFocused, 2=RunningBg, 3=Minimized
    property real scaleFactor: 1.0
    property real bounceOffset: 0.0
    property bool isHovered: false

    signal clicked()

    readonly property real baseSize: 40.0
    readonly property real iconSize: baseSize * scaleFactor

    width: iconSize
    height: iconSize
    y: -bounceOffset * 40.0

    // ── Outer hover highlight aura ──────────────────────────────
    Rectangle {
        anchors.centerIn: parent
        width: iconSize + 6
        height: iconSize + 6
        radius: (10.0 * root.scaleFactor) + 3
        color: Qt.rgba(1, 1, 1, 0.16)
        visible: root.isHovered
    }

    // ── Vector Icon Rendering ───────────────────────────────────
    Rectangle {
        id: iconBg
        anchors.centerIn: parent
        width: iconSize
        height: iconSize
        radius: 10.0 * root.scaleFactor
        clip: true

        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop {
                position: 0.0
                color: {
                    if (root.iconType === "folder")   return "#2F80ED";
                    if (root.iconType === "gear")     return "#8E8E93";
                    if (root.iconType === "barchart") return "#1C1C24";
                    if (root.iconType === "package")  return "#0A84FF";
                    return "#1A1B26"; // terminal default
                }
            }
            GradientStop {
                position: 1.0
                color: {
                    if (root.iconType === "folder")   return "#0056C6";
                    if (root.iconType === "gear")     return "#636366";
                    if (root.iconType === "barchart") return "#101018";
                    if (root.iconType === "package")  return "#0062D2";
                    return "#0D0E15";
                }
            }
        }

        border.color: Qt.rgba(1, 1, 1, 0.18)
        border.width: 1

        // ── Icon Glyphs ──
        // 1. Terminal: prompt chevron `>_`
        Text {
            anchors.centerIn: parent
            visible: root.iconType === "terminal"
            text: ">_"
            color: "#34C759"
            font.family: "monospace"
            font.bold: true
            font.pixelSize: Math.round(18 * root.scaleFactor)
        }

        // 2. Folder: folder tab + body
        Item {
            anchors.centerIn: parent
            width: 24 * root.scaleFactor
            height: 18 * root.scaleFactor
            visible: root.iconType === "folder"

            Rectangle {
                x: 0; y: 0
                width: parent.width * 0.45
                height: 5 * root.scaleFactor
                radius: 2 * root.scaleFactor
                color: Qt.rgba(1, 1, 1, 0.95)
            }
            Rectangle {
                x: 0; y: 3 * root.scaleFactor
                width: parent.width
                height: parent.height - 3 * root.scaleFactor
                radius: 3 * root.scaleFactor
                color: Qt.rgba(1, 1, 1, 0.95)
            }
        }

        // 3. Settings Gear: ⚙
        Text {
            anchors.centerIn: parent
            visible: root.iconType === "gear"
            text: "⚙"
            color: "#FFFFFF"
            font.pixelSize: Math.round(22 * root.scaleFactor)
        }

        // 4. Activity Monitor: pulse waves
        Canvas {
            anchors.fill: parent
            visible: root.iconType === "barchart"
            onPaint: {
                var ctx = getContext("2d");
                ctx.reset();
                var s = width / 40.0;
                ctx.strokeStyle = "#30D158";
                ctx.lineWidth = 2 * s;
                ctx.beginPath();
                ctx.moveTo(6 * s, 20 * s);
                ctx.lineTo(14 * s, 20 * s);
                ctx.lineTo(18 * s, 10 * s);
                ctx.lineTo(22 * s, 28 * s);
                ctx.lineTo(26 * s, 16 * s);
                ctx.lineTo(34 * s, 20 * s);
                ctx.stroke();
            }
        }

        // 5. App Store: package 'A' glyph
        Text {
            anchors.centerIn: parent
            visible: root.iconType === "package"
            text: "A"
            color: "#FFFFFF"
            font.bold: true
            font.pixelSize: Math.round(22 * root.scaleFactor)
        }
    }

    // ── Running indicator dot ───────────────────────────────────
    Item {
        id: indicatorContainer
        anchors.horizontalCenter: parent.horizontalCenter
        y: parent.height + 4
        visible: root.appState !== 0

        // Soft halo (focused only)
        Rectangle {
            anchors.centerIn: parent
            width: 9
            height: 9
            radius: 4.5
            color: Qt.rgba(1, 1, 1, 0.28)
            visible: root.appState === 1
        }

        // Core dot
        Rectangle {
            anchors.centerIn: parent
            width: root.appState === 1 ? 5 : (root.appState === 2 ? 5 : 4)
            height: width
            radius: width / 2.0
            color: root.appState === 1 ? "#FFFFFF" :
                   (root.appState === 2 ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.35))
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        z: 10
        onClicked: root.clicked()
    }
}
