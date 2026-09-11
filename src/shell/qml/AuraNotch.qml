// ============================================================================
// AuraNotch.qml — Dynamic Sloped Trapezoid Center Notch for tinexus-shell
// Ref: AuraNotchWidget.cpp (46px high, sloped \______/ geometry)
// ============================================================================
import QtQuick

Item {
    id: root
    width: 300
    height: 46

    readonly property real cx: width * 0.5
    readonly property real notchTopHalf: 136.0
    readonly property real notchBotHalf: 98.0
    readonly property real notchH: 46.0

    // ── Sloped Trapezoid Notch Canvas ───────────────────────────
    Canvas {
        id: notchCanvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            // Path: Top-left -> Top-right -> Bottom-right -> Bottom-left -> Close
            ctx.beginPath();
            ctx.moveTo(root.cx - root.notchTopHalf, 0);
            ctx.lineTo(root.cx + root.notchTopHalf, 0);
            ctx.lineTo(root.cx + root.notchBotHalf, root.notchH);
            ctx.lineTo(root.cx - root.notchBotHalf, root.notchH);
            ctx.closePath();

            ctx.fillStyle = Qt.rgba(36 / 255.0, 40 / 255.0, 54 / 255.0, 0.96);
            ctx.fill();

            // Rim lines (left slope, bottom, right slope)
            ctx.beginPath();
            ctx.moveTo(root.cx - root.notchTopHalf, 0);
            ctx.lineTo(root.cx - root.notchBotHalf, root.notchH);
            ctx.lineTo(root.cx + root.notchBotHalf, root.notchH);
            ctx.lineTo(root.cx + root.notchTopHalf, 0);

            ctx.lineWidth = 1.2;
            ctx.strokeStyle = Qt.rgba(1.0, 1.0, 1.0, 0.14);
            ctx.stroke();
        }
    }

    // ── Left: Time Pill Capsule (cx - 96.0, y: 11.0, w: 72.0, h: 24.0) ──────
    Rectangle {
        id: timePill
        x: root.cx - 96.0
        y: 11.0
        width: 72.0
        height: 24.0
        radius: 12.0
        color: Qt.rgba(20 / 255.0, 22 / 255.0, 30 / 255.0, 0.86)
        border.color: Qt.rgba(1.0, 1.0, 1.0, 0.10)
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: typeof bridge !== "undefined" ? bridge.currentTime : "12:00 PM"
            color: "#F5F5FA"
            font.pixelSize: 11
            font.bold: true
        }
    }

    // ── Center: Avatar Circle 'T' (cx, y: 23.0, radius: 14.5) ─────────────
    Rectangle {
        id: centerAvatar
        x: root.cx - 14.5
        y: 8.5
        width: 29.0
        height: 29.0
        radius: 14.5
        color: centerMouse.containsMouse ? "#38BDF8" : "#3B82F6"
        border.color: Qt.rgba(147 / 255.0, 197 / 255.0, 253 / 255.0, 0.70)
        border.width: 1.2

        Text {
            anchors.centerIn: parent
            text: "T"
            color: "#FFFFFF"
            font.pixelSize: 13
            font.bold: true
        }

        MouseArea {
            id: centerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (typeof bridge !== "undefined") {
                    bridge.onNotchCenterClicked();
                }
            }
        }
    }

    // ── Right: Date Pill Capsule (cx + 24.0, y: 11.0, w: 72.0, h: 24.0) ─────
    Rectangle {
        id: datePill
        x: root.cx + 24.0
        y: 11.0
        width: 72.0
        height: 24.0
        radius: 12.0
        readonly property bool isOpen: typeof bridge !== "undefined" && bridge.calendarOpen
        color: (dateMouse.containsMouse || isOpen) ? Qt.rgba(1.0, 1.0, 1.0, 0.15)
                                                   : Qt.rgba(20 / 255.0, 22 / 255.0, 30 / 255.0, 0.86)
        border.color: Qt.rgba(1.0, 1.0, 1.0, 0.10)
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: typeof bridge !== "undefined" ? bridge.currentDate : "Sep 10"
            color: (dateMouse.containsMouse || datePill.isOpen) ? "#38BDF8" : "#F5F5FA"
            font.pixelSize: 11
            font.bold: true
        }

        MouseArea {
            id: dateMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (typeof bridge !== "undefined") {
                    bridge.toggleCalendar();
                }
            }
        }
    }
}
