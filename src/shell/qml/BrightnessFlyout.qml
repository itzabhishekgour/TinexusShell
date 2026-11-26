// ============================================================================
// BrightnessFlyout.qml — Tactile Control-Center Pill Display Flyout
// ============================================================================
import QtQuick
import QtQuick.Layouts
import "../../common/qml"

Item {
    id: root
    readonly property int shadowPad: 18
    readonly property int cardWidth: 272
    readonly property int cardHeight: 122

    property int localBrightness: (typeof bridge !== "undefined") ? bridge.brightness : 80

    Connections {
        target: typeof bridge !== "undefined" ? bridge : null
        function onBrightnessChanged() {
            if (!sliderMouseArea.pressed) {
                root.localBrightness = bridge.brightness
            }
        }
    }

    width: cardWidth + shadowPad * 2
    height: cardHeight + shadowPad * 2

    Item {
        id: glassCard
        x: root.shadowPad
        y: root.shadowPad
        width: root.cardWidth
        height: root.cardHeight

        LiquidGlass {
            anchors.fill: parent
            materialType: "popover"
            cornerRadius: 14
            blurAmount: 28
            fluidOpacity: 0.68
            shadowEnabled: true
            topSpecularOpacity: 0.14
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            // Header
            RowLayout {
                Layout.fillWidth: true
                height: 22
                spacing: 8

                Image {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    sourceSize.width: 16
                    sourceSize.height: 16
                    fillMode: Image.PreserveAspectFit
                    source: "image://icon/display-brightness-symbolic?color=#FF9F0A"
                }

                Text {
                    text: "Display"
                    color: "#FFFFFF"
                    font.family: "Inter"
                    font.pixelSize: 14
                    font.bold: true
                    Layout.fillWidth: true
                }

                Text {
                    text: root.localBrightness + "%"
                    color: Qt.rgba(1, 1, 1, 0.65)
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // Zero-Lag Tactile Pill Slider
            Item {
                Layout.fillWidth: true
                height: 36

                Rectangle {
                    id: sliderTrack
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 28
                    radius: 14
                    color: Qt.rgba(0, 0, 0, 0.42)
                    border.color: Qt.rgba(1, 1, 1, 0.12)
                    border.width: 1
                    clip: true

                    readonly property real ratio: Math.max(0.0, Math.min(1.0, (root.localBrightness - 5) / 95.0))

                    Rectangle {
                        id: sliderFill
                        height: parent.height
                        width: parent.height + (parent.width - parent.height) * sliderTrack.ratio
                        radius: 14
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: "#FF9F0A" }
                            GradientStop { position: 1.0; color: "#FFD60A" }
                        }
                    }

                    Image {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 13
                        height: 13
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/display-brightness-symbolic?color=#FFFFFF"
                    }

                    Rectangle {
                        width: 22
                        height: 22
                        radius: 11
                        anchors.verticalCenter: parent.verticalCenter
                        x: Math.max(3, sliderFill.width - width - 3)
                        color: "#FFFFFF"
                        border.color: Qt.rgba(0, 0, 0, 0.16)
                        border.width: 1
                        scale: sliderMouseArea.pressed ? 1.08 : 1.0
                        Behavior on scale { SpringAnimation { spring: 6.5; damping: 0.72 } }
                    }

                    MouseArea {
                        id: sliderMouseArea
                        anchors.fill: parent
                        preventStealing: true
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        function applyFromMouse(mx) {
                            var pad = 14
                            var usable = Math.max(1, sliderTrack.width - pad * 2)
                            var norm = Math.max(0.0, Math.min(1.0, (mx - pad) / usable))
                            var pct = Math.max(5, Math.min(100, Math.round(5 + norm * 95)))
                            root.localBrightness = pct
                            if (typeof bridge !== "undefined") {
                                bridge.brightness = pct
                            }
                        }

                        onPressed: function(mouse) {
                            if (typeof bridge !== "undefined") bridge.beginBrightnessDrag()
                            applyFromMouse(mouse.x)
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed) applyFromMouse(mouse.x)
                        }
                        onReleased: {
                            if (typeof bridge !== "undefined") bridge.endBrightnessDrag()
                        }
                        onCanceled: {
                            if (typeof bridge !== "undefined") bridge.endBrightnessDrag()
                        }
                    }
                }
            }
        }
    }
}
