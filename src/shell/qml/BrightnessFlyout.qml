// ============================================================================
// BrightnessFlyout.qml — Display & Brightness Control Flyout with LiquidGlass
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: 250
    height: 106

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
        Row {
            width: parent.width
            spacing: 8

            Image {
                width: 16
                height: 16
                anchors.verticalCenter: parent.verticalCenter
                fillMode: Image.PreserveAspectFit
                source: "image://icon/display-brightness-symbolic?color=#FF9F0A"
            }

            Text {
                text: "Display"
                color: "#FFFFFF"
                font.family: "Inter"
                font.pixelSize: 14
                font.bold: true
                anchors.verticalCenter: parent.verticalCenter
            }

            Item { width: 10 }

            Text {
                text: (typeof bridge !== "undefined" ? bridge.brightness : 80) + "%"
                color: Qt.rgba(1, 1, 1, 0.60)
                font.family: "Inter"
                font.pixelSize: 12
                anchors.verticalCenter: parent.verticalCenter
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
                width: Math.max(height, parent.width * ((typeof bridge !== "undefined" ? bridge.brightness : 80) / 100.0))
                color: "#FF9F0A"
            }

            MouseArea {
                id: sliderMouseArea
                anchors.fill: parent
                preventStealing: true
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                function updateBrightness(mouseX) {
                    var pct = Math.max(5, Math.min(100, Math.round((mouseX / sliderTrack.width) * 100)));
                    if (typeof bridge !== "undefined") {
                        bridge.brightness = pct;
                    }
                }

                onPositionChanged: function(mouse) {
                    if (pressed) {
                        updateBrightness(mouse.x);
                    }
                }
                onPressed: function(mouse) {
                    updateBrightness(mouse.x);
                }
                onClicked: function(mouse) {
                    updateBrightness(mouse.x);
                }
            }
        }
    }
}
