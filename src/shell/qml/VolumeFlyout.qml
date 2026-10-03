// ============================================================================
// VolumeFlyout.qml — Sound & Volume Control Flyout with LiquidGlass
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: 250
    height: 160

    LiquidGlass {
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 12
    }

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // Header
        Item {
            width: parent.width
            height: 20

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
                font.pixelSize: 12
            }
        }

        // Slider track + thumb
        Rectangle {
            id: sliderTrack
            width: parent.width
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
                onPositionChanged: function(mouse) {
                    var pct = Math.max(0, Math.min(100, Math.round((mouse.x / width) * 100)));
                    if (typeof bridge !== "undefined") {
                        bridge.volume = pct;
                    }
                }
                onClicked: function(mouse) {
                    var pct = Math.max(0, Math.min(100, Math.round((mouse.x / width) * 100)));
                    if (typeof bridge !== "undefined") {
                        bridge.volume = pct;
                    }
                }
            }
        }

        // Output Device Pill
        Rectangle {
            width: parent.width
            height: 32
            radius: 8
            color: Qt.rgba(1, 1, 1, 0.08)
            border.color: Qt.rgba(1, 1, 1, 0.10)

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
                Text { text: "Internal Speakers"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }
}
