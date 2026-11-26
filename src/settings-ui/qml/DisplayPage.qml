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

        // ── Monitor Graphic Preview ───────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 124
            radius: 12
            color: Qt.rgba(0.16, 0.17, 0.21, 0.78)
            border.color: Qt.rgba(1, 1, 1, 0.11)
            border.width: 1

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 5

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 110
                    height: 68
                    radius: 5
                    color: "#0a0a0e"
                    border.color: "#48484c"
                    border.width: 2

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 3
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#1e3a8a" }
                            GradientStop { position: 0.5; color: typeof bridge !== "undefined" ? bridge.accentColor : "#0A84FF" }
                            GradientStop { position: 1.0; color: "#38bdf8" }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: typeof bridge !== "undefined" ? bridge.displayResolution : "1920 × 1080"
                            color: "#e6ffffff"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 28
                    height: 10
                    radius: 2
                    color: "#48484c"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: (typeof bridge !== "undefined" && bridge.displaySubtitle.length > 0) ? bridge.displaySubtitle : "Primary Display"
                    color: "#9898a0"
                    font.pixelSize: 11
                }
            }
        }

        Text {
            text: "RESOLUTION & COLOR"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card: Display Controls ────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: dispCol.implicitHeight

            ColumnLayout {
                id: dispCol
                anchors.fill: parent
                spacing: 0

                MacSettingRow {
                    title: "Resolution"
                    subtitle: "Live DRM/KMS mode for " + (typeof bridge !== "undefined" ? bridge.displayConnector : "Primary Display")
                    showSeparator: true

                    MacComboBox {
                        model: (typeof bridge !== "undefined" && bridge.availableResolutions.length > 0) ? bridge.availableResolutions : ["1920 × 1080"]
                        currentIndex: typeof bridge !== "undefined" ? bridge.currentResolutionIndex : 0
                        onActivated: (index) => bridge.setDisplayResolutionIndex(index)
                    }
                }

                MacSettingRow {
                    title: "Refresh Rate"
                    subtitle: "Output scanout frequency in Hz"
                    showSeparator: true

                    MacComboBox {
                        model: (typeof bridge !== "undefined" && bridge.availableRefreshRates.length > 0) ? bridge.availableRefreshRates : ["60 Hz"]
                        currentIndex: typeof bridge !== "undefined" ? bridge.currentRefreshIndex : 0
                        onActivated: (index) => bridge.setDisplayRefreshIndex(index)
                    }
                }

                MacSettingRow {
                    title: "Display Scaling"
                    subtitle: "Fractional HiDPI Wayland output scaling"
                    showSeparator: true

                    MacComboBox {
                        model: ["100% (Native)", "125% (Balanced)", "150% (Sharp)", "200% (HiDPI)"]
                        currentIndex: bridge.displayScaleIndex
                        onActivated: (index) => bridge.setDisplayScaleIndex(index)
                    }
                }

                Item {
                    Layout.fillWidth: true
                    height: 48

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        Text {
                            text: "Screen Brightness"
                            color: "#f5f5f7"
                            font.pixelSize: 13
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            Image {
                                width: 14; height: 14
                                source: "image://tinexus/display-brightness-symbolic"
                                sourceSize: Qt.size(14, 14)
                                opacity: 0.55
                            }

                            MacSlider {
                                id: brightnessSlider
                                width: 210
                                from: 10
                                to: 100
                                value: bridge.brightness
                                onMoved: (val) => bridge.setBrightness(Math.round(val))
                            }

                            Image {
                                width: 16; height: 16
                                source: "image://tinexus/display-brightness-symbolic"
                                sourceSize: Qt.size(16, 16)
                            }

                            Text {
                                width: 42
                                horizontalAlignment: Text.AlignRight
                                text: Math.round(brightnessSlider.value) + "%"
                                color: "#86868b"
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
                    title: "Night Light"
                    subtitle: "Apply warm 4000K gamma curve to reduce eye strain"
                    showSeparator: true

                    MacSwitch {
                        checked: bridge.nightLight
                        onToggled: (val) => bridge.setNightLight(val)
                    }
                }

                MacSettingRow {
                    title: "Variable Refresh Rate (VRR)"
                    subtitle: bridge.vrrSupported ? "Adaptive sync supported by active display" : "Fixed refresh display (hardware VRR unsupported)"
                    showSeparator: false

                    MacSwitch {
                        checked: bridge.vrrEnabled
                        enabled: bridge.vrrSupported
                        onToggled: (val) => bridge.setVrrEnabled(val)
                    }
                }
            }
        }

        Text {
            text: "HARDWARE DETAILS"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        MacCard {
            Layout.fillWidth: true
            implicitHeight: hwCol.implicitHeight

            ColumnLayout {
                id: hwCol
                anchors.fill: parent
                spacing: 0

                MacSettingRow {
                    title: "Compositor Engine"
                    subtitle: typeof bridge !== "undefined" ? bridge.compositorInfo : "tinexus-comp (wlroots 0.19)"
                    showSeparator: false

                    Text {
                        text: "Active"
                        color: "#30d158"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
