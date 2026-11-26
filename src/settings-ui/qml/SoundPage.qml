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

        // ── Section 1: Output ─────────────────────────────────────────────
        Text {
            text: "OUTPUT"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        MacCard {
            Layout.fillWidth: true
            implicitHeight: outputCol.implicitHeight

            ColumnLayout {
                id: outputCol
                anchors.fill: parent
                spacing: 0

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
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            Image {
                                width: 15; height: 15
                                source: "image://tinexus/" + (bridge.muted ? "audio-volume-muted-symbolic" : "audio-volume-low-symbolic")
                                sourceSize: Qt.size(15, 15)
                                opacity: 0.7
                            }

                            MacSlider {
                                id: volumeSlider
                                width: 210
                                from: 0
                                to: 100
                                value: bridge.volume
                                enabled: !bridge.muted
                                onMoved: (val) => bridge.setVolume(Math.round(val))
                            }

                            Image {
                                width: 16; height: 16
                                source: "image://tinexus/audio-volume-high-symbolic"
                                sourceSize: Qt.size(16, 16)
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

                    Rectangle {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#12ffffff"
                    }
                }

                MacSettingRow {
                    title: "Mute Output"
                    subtitle: "Silence all desktop audio streams"
                    showSeparator: true

                    MacSwitch {
                        checked: bridge.muted
                        onToggled: (val) => bridge.setMuted(val)
                    }
                }

                MacSettingRow {
                    title: "Output Device"
                    subtitle: "Active PipeWire / PulseAudio sink"
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

        // ── Section 2: Input / Microphone ─────────────────────────────────
        Text {
            text: "INPUT"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        MacCard {
            Layout.fillWidth: true
            implicitHeight: inputCol.implicitHeight

            ColumnLayout {
                id: inputCol
                anchors.fill: parent
                spacing: 0

                Item {
                    Layout.fillWidth: true
                    height: 48

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        Text {
                            text: "Input Volume"
                            color: "#f5f5f7"
                            font.pixelSize: 13
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            Image {
                                width: 15; height: 15
                                source: "image://tinexus/" + (bridge.inputMuted ? "microphone-sensitivity-muted-symbolic" : "audio-input-microphone-symbolic")
                                sourceSize: Qt.size(15, 15)
                                opacity: 0.75
                            }

                            MacSlider {
                                id: inputSlider
                                width: 210
                                from: 0
                                to: 100
                                value: bridge.inputVolume
                                enabled: !bridge.inputMuted
                                onMoved: (val) => bridge.setInputVolume(Math.round(val))
                            }

                            Text {
                                width: 44
                                horizontalAlignment: Text.AlignRight
                                text: bridge.inputMuted ? "Muted" : (Math.round(inputSlider.value) + "%")
                                color: bridge.inputMuted ? "#ff453a" : "#86868b"
                                font.pixelSize: 11
                            }
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#12ffffff"
                    }
                }

                MacSettingRow {
                    title: "Mute Microphone"
                    subtitle: "Mute default PipeWire audio capture source"
                    showSeparator: true

                    MacSwitch {
                        checked: bridge.inputMuted
                        onToggled: (val) => bridge.setInputMuted(val)
                    }
                }

                MacSettingRow {
                    title: "Input Device"
                    subtitle: "Active microphone / capture source"
                    showSeparator: false

                    MacComboBox {
                        model: (bridge.inputDevices && bridge.inputDevices.length > 0) ? bridge.inputDevices : ["No Audio Input Devices Available"]
                        currentIndex: bridge.currentInputIndex
                        enabled: (bridge.inputDevices && bridge.inputDevices.length > 0 && bridge.inputDevices[0] !== "No Audio Input Devices Available")
                        onActivated: (index) => bridge.setInputDevice(index)
                    }
                }
            }
        }

        // ── Section 3: Sound Effects ──────────────────────────────────────
        Text {
            text: "SOUND EFFECTS"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        MacCard {
            Layout.fillWidth: true
            implicitHeight: effectsCol.implicitHeight

            ColumnLayout {
                id: effectsCol
                anchors.fill: parent
                spacing: 0

                MacSettingRow {
                    title: "Alert & Notification Sounds"
                    subtitle: "Auditory feedback via PipeWire / PulseAudio"
                    showSeparator: false

                    RowLayout {
                        spacing: 8
                        MacButton {
                            text: "Refresh"
                            onClicked: bridge.refreshAudioDevices()
                        }
                        MacButton {
                            text: "Play Test Sound"
                            primary: true
                            onClicked: bridge.playTestSound()
                        }
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
