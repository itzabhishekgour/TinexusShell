// ============================================================================
// VolumeFlyout.qml — Sound & Volume Control Flyout with LiquidGlass
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    readonly property int shadowPad: 18
    readonly property int cardWidth: 256
    readonly property int cardHeight: 174

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
            fluidOpacity: 0.66
            shadowEnabled: true
            topSpecularOpacity: 0.14
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            // Header
            Item {
                width: parent.width
                height: 22

                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Image {
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/" + (typeof bridge !== "undefined" && (bridge.soundMuted || bridge.volume === 0)
                                ? "audio-volume-muted-symbolic?color=#FF453A"
                                : "audio-volume-high-symbolic?color=#38BDF8")
                    }

                    Text {
                        text: "Sound"
                        color: "#FFFFFF"
                        font.family: "Inter"
                        font.pixelSize: 14
                        font.bold: true
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }

                Text {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: typeof bridge !== "undefined" ? (bridge.soundMuted ? "Muted" : (bridge.volume + "%")) : "75%"
                    color: Qt.rgba(1, 1, 1, 0.60)
                    font.family: "Inter"
                    font.pixelSize: 12
                }
            }

            Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // Slider track + thumb
            Rectangle {
                width: parent.width
                height: 36
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.045)

                Rectangle {
                    id: sliderTrack
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    height: 24
                    radius: 12
                    color: Qt.rgba(0, 0, 0, 0.40)
                    border.color: Qt.rgba(1, 1, 1, 0.10)

                    Rectangle {
                        id: sliderProgress
                        height: parent.height
                        radius: 12
                        width: Math.max(height, parent.width * ((typeof bridge !== "undefined" ? bridge.volume : 75) / 100.0))
                        color: "#6B8CEF"
                    }

                    MouseArea {
                        anchors.fill: parent
                        preventStealing: true
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        function updateVolume(mouseX) {
                            var pct = Math.max(0, Math.min(100, Math.round((mouseX / sliderTrack.width) * 100)));
                            if (typeof bridge !== "undefined") {
                                bridge.volume = pct;
                            }
                        }

                        onPressed: function(mouse) {
                            updateVolume(mouse.x);
                        }
                        onPositionChanged: function(mouse) {
                            if (pressed) {
                                updateVolume(mouse.x);
                            }
                        }
                        onClicked: function(mouse) {
                            updateVolume(mouse.x);
                        }
                    }
                }
            }

            Rectangle { width: parent.width; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // Output Device Pill
            Rectangle {
                width: parent.width
                height: 32
                radius: 8
                color: Qt.rgba(1, 1, 1, 0.025)
                border.color: Qt.rgba(1, 1, 1, 0.08)

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Image {
                        width: 14
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/audio-speakers-symbolic?color=#38BDF8"
                    }
                    Text {
                        text: "Internal Speakers"
                        color: "#FFFFFF"
                        font.family: "Inter"
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }
}
