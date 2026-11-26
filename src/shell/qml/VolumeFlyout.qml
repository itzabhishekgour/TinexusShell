// ============================================================================
// VolumeFlyout.qml — Tactile Control-Center Pill Sound Flyout with LiquidGlass
// ============================================================================
import QtQuick
import QtQuick.Layouts
import "../../common/qml"

Item {
    id: root
    readonly property int shadowPad: 18
    readonly property int cardWidth: 272
    readonly property int cardHeight: 174

    property int localVolume: (typeof bridge !== "undefined") ? bridge.volume : 75

    Connections {
        target: typeof bridge !== "undefined" ? bridge : null
        function onVolumeChanged() {
            if (!sliderMouseArea.pressed) {
                root.localVolume = bridge.volume
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
                height: 24
                spacing: 8

                Rectangle {
                    width: 24
                    height: 24
                    radius: 6
                    color: muteMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : "transparent"

                    Image {
                        anchors.centerIn: parent
                        width: 16
                        height: 16
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/" + (typeof bridge !== "undefined" && (bridge.soundMuted || root.localVolume === 0)
                                ? "audio-volume-muted-symbolic?color=#FF453A"
                                : "audio-volume-high-symbolic?color=#38BDF8")
                    }

                    MouseArea {
                        id: muteMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") {
                                bridge.soundMuted = !bridge.soundMuted
                            }
                        }
                    }
                }

                Text {
                    text: "Sound"
                    color: "#FFFFFF"
                    font.family: "Inter"
                    font.pixelSize: 14
                    font.bold: true
                    Layout.fillWidth: true
                }

                Text {
                    text: typeof bridge !== "undefined" ? (bridge.soundMuted ? "Muted" : (root.localVolume + "%")) : (root.localVolume + "%")
                    color: Qt.rgba(1, 1, 1, 0.65)
                    font.family: "Inter"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // Tactile macOS-style Pill Slider
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

                    readonly property real ratio: Math.max(0.0, Math.min(1.0, root.localVolume / 100.0))

                    Rectangle {
                        id: sliderFill
                        height: parent.height
                        width: parent.height + (parent.width - parent.height) * sliderTrack.ratio
                        radius: 14
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: (typeof bridge !== "undefined" && bridge.soundMuted) ? "#5E5E66" : "#4A7DFF" }
                            GradientStop { position: 1.0; color: (typeof bridge !== "undefined" && bridge.soundMuted) ? "#7A7A85" : "#38BDF8" }
                        }
                    }

                    // Embedded Glyph inside Track
                    Image {
                        x: 8
                        anchors.verticalCenter: parent.verticalCenter
                        width: 13
                        height: 13
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/" + (typeof bridge !== "undefined" && (bridge.soundMuted || root.localVolume === 0)
                                ? "audio-volume-muted-symbolic?color=#FFFFFF"
                                : "audio-volume-medium-symbolic?color=#FFFFFF")
                    }

                    // Smooth Circular Knob at Fill Edge
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
                            var pct = Math.round(norm * 100)
                            root.localVolume = pct
                            if (typeof bridge !== "undefined") {
                                bridge.volume = pct
                            }
                        }

                        onPressed: function(mouse) {
                            if (typeof bridge !== "undefined") bridge.beginVolumeDrag()
                            applyFromMouse(mouse.x)
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed) applyFromMouse(mouse.x)
                        }
                        onReleased: {
                            if (typeof bridge !== "undefined") bridge.endVolumeDrag()
                        }
                        onCanceled: {
                            if (typeof bridge !== "undefined") bridge.endVolumeDrag()
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // Real Active Output Device Pill
            Rectangle {
                Layout.fillWidth: true
                height: 32
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.03)
                border.color: Qt.rgba(1, 1, 1, 0.08)

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Image {
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        sourceSize.width: 14
                        sourceSize.height: 14
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/audio-speakers-symbolic?color=#38BDF8"
                    }

                    Text {
                        text: (typeof bridge !== "undefined" && bridge.outputDeviceName.length > 0)
                              ? bridge.outputDeviceName
                              : "Audio Output"
                        color: "#FFFFFF"
                        font.family: "Inter"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
