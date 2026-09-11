// ============================================================================
// MacSquircleIcon.qml — High-fidelity macOS / TxUI Vector Badge Icon
// Ref: TxUI SettingsWidget::draw_icon_* methods (SettingsWidget.cpp:1509-1565)
// ============================================================================
import QtQuick
import QtQuick.Shapes

Rectangle {
    id: root
    property int iconSize: 20
    property color iconColor: "#0A84FF"
    property string iconId: ""
    property string iconText: ""
    property int glyphSize: 11

    width: iconSize
    height: iconSize
    radius: Math.round(iconSize * 0.22) // ~4.5px radius for 20px squircle
    clip: true

    // Subtle vertical gradient for macOS glossy/depth look
    gradient: Gradient {
        GradientStop { position: 0.0; color: Qt.lighter(root.iconColor, 1.15) }
        GradientStop { position: 1.0; color: root.iconColor }
    }

    // Top half subtle glossy sheen
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.floor(parent.height * 0.5)
        radius: root.radius
        color: Qt.rgba(1, 1, 1, 0.10)
    }

    // Subtle 1px inner highlight border for depth
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: "transparent"
        border.color: Qt.rgba(1, 1, 1, 0.20)
        border.width: 1
    }

    // ── Vector Badges (Resolution-independent) ───────────────────────
    Item {
        anchors.centerIn: parent
        width: 14
        height: 14

        // 1. Display: Monitor Screen + Stand
        Item {
            anchors.fill: parent
            visible: root.iconId === "display"

            Rectangle {
                x: 1; y: 2; width: 12; height: 8; radius: 1
                color: "#FFFFFF"
                Rectangle { x: 1; y: 1; width: 10; height: 6; color: Qt.darker(root.iconColor, 1.2) }
            }
            Rectangle { x: 6; y: 10; width: 2; height: 2; color: "#FFFFFF" }
            Rectangle { x: 4; y: 12; width: 6; height: 1; radius: 0.5; color: "#FFFFFF" }
        }

        // 2. Sound: Speaker Cone + Waves
        Item {
            anchors.fill: parent
            visible: root.iconId === "sound"

            // Speaker body
            Rectangle { x: 2; y: 4.5; width: 3; height: 5; color: "#FFFFFF" }
            Shape {
                anchors.fill: parent
                ShapePath {
                    fillColor: "#FFFFFF"
                    strokeColor: "transparent"
                    startX: 5; startY: 4.5
                    PathLine { x: 8; y: 2 }
                    PathLine { x: 8; y: 12 }
                    PathLine { x: 5; y: 9.5 }
                    PathLine { x: 5; y: 4.5 }
                }
            }
            // Sound wave arc
            Shape {
                anchors.fill: parent
                ShapePath {
                    fillColor: "transparent"
                    strokeColor: "#FFFFFF"
                    strokeWidth: 1.2
                    capStyle: ShapePath.RoundCap
                    startX: 10; startY: 4
                    PathQuad { x: 10; y: 10; controlX: 12.5; controlY: 7 }
                }
            }
        }

        // 3. Appearance / Theme: 4 Palette Color Circles
        Item {
            anchors.fill: parent
            visible: root.iconId === "appearance"

            Rectangle { x: 2; y: 2; width: 4; height: 4; radius: 2; color: "#FFFFFF" }
            Rectangle { x: 8; y: 2; width: 4; height: 4; radius: 2; color: "#FFD60A" }
            Rectangle { x: 2; y: 8; width: 4; height: 4; radius: 2; color: "#30D158" }
            Rectangle { x: 8; y: 8; width: 4; height: 4; radius: 2; color: "#64D2FF" }
        }

        // 4. Wi-Fi / Network: Signal Bars
        Item {
            anchors.fill: parent
            visible: root.iconId === "wifi"

            Rectangle { x: 2;  y: 10; width: 2; height: 2; radius: 0.5; color: "#FFFFFF" }
            Rectangle { x: 5;  y: 7;  width: 2; height: 5; radius: 0.5; color: "#FFFFFF" }
            Rectangle { x: 8;  y: 4.5; width: 2; height: 7.5; radius: 0.5; color: "#FFFFFF" }
            Rectangle { x: 11; y: 2;  width: 2; height: 10; radius: 0.5; color: "#FFFFFF" }
        }

        // 5. Power / Battery
        Item {
            anchors.fill: parent
            visible: root.iconId === "power"

            Rectangle {
                x: 1; y: 3.5; width: 10; height: 7; radius: 1.5
                color: "transparent"
                border.color: "#FFFFFF"
                border.width: 1.2
                Rectangle { x: 1.5; y: 1.5; width: 5; height: 4; radius: 0.5; color: "#FFFFFF" }
            }
            Rectangle { x: 11.5; y: 5.5; width: 1.5; height: 3; radius: 0.5; color: "#FFFFFF" }
        }

        // 6. Keyboard
        Item {
            anchors.fill: parent
            visible: root.iconId === "keyboard"

            Rectangle {
                x: 1; y: 3; width: 12; height: 8; radius: 1.5
                color: "#FFFFFF"
                Rectangle { x: 1; y: 1; width: 10; height: 6; color: Qt.darker(root.iconColor, 1.2) }
                Rectangle { x: 3; y: 5.5; width: 6; height: 1.2; color: "#FFFFFF" }
            }
        }

        // 7. Privacy & Security: Padlock
        Item {
            anchors.fill: parent
            visible: root.iconId === "privacy"

            // Shackle
            Rectangle {
                x: 4.5; y: 1.5; width: 5; height: 5; radius: 2.5
                color: "transparent"
                border.color: "#FFFFFF"
                border.width: 1.2
            }
            // Lock body
            Rectangle {
                x: 3; y: 5.5; width: 8; height: 6.5; radius: 1.5
                color: "#FFFFFF"
                // Keyhole
                Rectangle {
                    anchors.centerIn: parent
                    width: 1.5; height: 2.5; radius: 0.75
                    color: Qt.darker(root.iconColor, 1.3)
                }
            }
        }

        // 8. General / About: Info 'i' glyph
        Text {
            anchors.centerIn: parent
            visible: root.iconId === "about"
            text: "i"
            font.family: "Serif"
            font.bold: true
            font.pixelSize: 11
            color: "#FFFFFF"
        }

        // Fallback text glyph if iconId not set
        Text {
            anchors.centerIn: parent
            visible: root.iconId === "" && root.iconText !== ""
            text: root.iconText
            color: "#FFFFFF"
            font.pixelSize: root.glyphSize
        }
    }
}
