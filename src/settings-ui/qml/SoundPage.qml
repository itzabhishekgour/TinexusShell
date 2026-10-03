import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    ColumnLayout {
        width: Math.min(parent.width - 48, 680)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14

        Item { height: 4 }

        // Section Title: Output
        Text {
            text: "OUTPUT"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card: Sound Output Controls ───────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: outputCol.implicitHeight

            ColumnLayout {
                id: outputCol
                anchors.fill: parent
                spacing: 0

                // Row 1: Output Volume with inline slider
                Item {
                    Layout.fillWidth: true
                    height: 48

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        Text {
                            text: "Output Volume"
                            color: "#f5f5f7"
                            font.pixelSize: 13
                            font.weight: Font.Normal
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            // Low volume speaker
                            Item {
                                width: 12; height: 12
                                Rectangle { x: 1; y: 4; width: 3; height: 4; color: "#86868b" }
                                Canvas {
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.fillStyle = "#86868b";
                                        ctx.beginPath();
                                        ctx.moveTo(4, 4);
                                        ctx.lineTo(8, 1);
                                        ctx.lineTo(8, 11);
                                        ctx.lineTo(4, 8);
                                        ctx.closePath();
                                        ctx.fill();
                                    }
                                }
                            }

                            MacSlider {
                                id: volumeSlider
                                width: 220
                                from: 0
                                to: 100
                                value: bridge.volume
                                enabled: !bridge.muted
                                onMoved: (val) => bridge.setVolume(Math.round(val))
                            }

                            // High volume speaker
                            Item {
                                width: 14; height: 12
                                Rectangle { x: 1; y: 4; width: 3; height: 4; color: "#f5f5f7" }
                                Canvas {
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.fillStyle = "#f5f5f7";
                                        ctx.beginPath();
                                        ctx.moveTo(4, 4);
                                        ctx.lineTo(8, 1);
                                        ctx.lineTo(8, 11);
                                        ctx.lineTo(4, 8);
                                        ctx.closePath();
                                        ctx.fill();
                                        ctx.strokeStyle = "#f5f5f7";
                                        ctx.lineWidth = 1.2;
                                        ctx.beginPath();
                                        ctx.arc(8, 6, 4, -0.6, 0.6);
                                        ctx.stroke();
                                    }
                                }
                            }

                            Text {
                                width: 44
                                horizontalAlignment: Text.AlignRight
                                text: bridge.muted ? "Muted" : (Math.round(volumeSlider.value) + "%")
                                color: bridge.muted ? "#ff453a" : "#86868b"
                                font.pixelSize: 11
                            }
                        }
                    }

                    // Hairline separator
                    Rectangle {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#12ffffff"
                    }
                }

                // Row 2: Mute Output
                MacSettingRow {
                    title: "Mute Output"
                    subtitle: "Silence all desktop audio streams"
                    showSeparator: true

                    MacSwitch {
                        checked: bridge.muted
                        onToggled: (val) => bridge.setMuted(val)
                    }
                }

                // Row 3: Output Device Dropdown
                MacSettingRow {
                    title: "Output Device"
                    subtitle: "Active PipeWire / ALSA audio sink"
                    showSeparator: false

                    MacComboBox {
                        model: (bridge.outputDevices && bridge.outputDevices.length > 0) ? bridge.outputDevices : ["No Audio Output Devices Available"]
                        currentIndex: bridge.currentOutputIndex
                        enabled: (bridge.outputDevices && bridge.outputDevices.length > 0 && bridge.outputDevices[0] !== "No Audio Output Devices Available")
                        onActivated: (index) => bridge.setOutputDevice(index)
                    }
                }
            }
        }

        // Section Title: Sound Effects
        Text {
            text: "SOUND EFFECTS"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card: Alert Sounds ────────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: effectsCol.implicitHeight

            ColumnLayout {
                id: effectsCol
                anchors.fill: parent
                spacing: 0

                MacSettingRow {
                    title: "Alert & Notification Sounds"
                    subtitle: "Auditory feedback on system events"
                    showSeparator: false

                    MacButton {
                        text: "Play Test Sound"
                        onClicked: bridge.playTestSound()
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
