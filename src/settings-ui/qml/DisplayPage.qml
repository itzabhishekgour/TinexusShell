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

        // ── Monitor Graphic Preview (macOS Displays Top Graphic) ──────────
        Rectangle {
            Layout.fillWidth: true
            height: 124
            radius: 10
            color: "#28282a"
            border.color: "#14ffffff"
            border.width: 1

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 5

                // Miniature Display Frame
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 110
                    height: 68
                    radius: 5
                    color: "#0a0a0e"
                    border.color: "#48484c"
                    border.width: 2

                    // Screen with wallpaper preview
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 3
                        radius: 3
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#1e3a8a" }
                            GradientStop { position: 0.5; color: "#0A84FF" }
                            GradientStop { position: 1.0; color: "#38bdf8" }
                        }

                        Text {
                            anchors.centerIn: parent
                            text: typeof bridge !== "undefined" ? bridge.displayResolution : "1920 × 1080"
                            color: "#ccffffff"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                }

                // Monitor Stand
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    width: 28
                    height: 10
                    radius: 2
                    color: "#48484c"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: typeof bridge !== "undefined" ? bridge.displaySubtitle : "Primary Display  •  60 Hz"
                    color: "#86868b"
                    font.pixelSize: 11
                }
            }
        }

        // Section Title: Resolution & Color
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

                // Row 1: Display Scaling Dropdown
                MacSettingRow {
                    title: "Display Scaling"
                    subtitle: "Adjust scale factor for high-DPI Wayland outputs"
                    showSeparator: true

                    MacComboBox {
                        model: ["100% (Native)", "125% (Balanced)", "150% (Sharp)", "200% (HiDPI)"]
                        currentIndex: bridge.displayScaleIndex
                        onActivated: (index) => bridge.setDisplayScaleIndex(index)
                    }
                }

                // Row 2: Screen Brightness inline slider
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
                            font.weight: Font.Normal
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            // Small sun (low brightness)
                            Item {
                                width: 12; height: 12
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 6; height: 6; radius: 3
                                    color: "#86868b"
                                }
                            }

                            MacSlider {
                                id: brightnessSlider
                                width: 220
                                from: 10
                                to: 100
                                value: bridge.brightness
                                onMoved: (val) => bridge.setBrightness(Math.round(val))
                            }

                            // Large radiating sun (high brightness)
                            Item {
                                width: 14; height: 14
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 7; height: 7; radius: 3.5
                                    color: "#f5f5f7"
                                }
                                Canvas {
                                    anchors.fill: parent
                                    onPaint: {
                                        var ctx = getContext("2d");
                                        ctx.reset();
                                        ctx.strokeStyle = "#f5f5f7";
                                        ctx.lineWidth = 1.0;
                                        // 4 cross rays
                                        ctx.beginPath();
                                        ctx.moveTo(7, 1); ctx.lineTo(7, 3);
                                        ctx.moveTo(7, 11); ctx.lineTo(7, 13);
                                        ctx.moveTo(1, 7); ctx.lineTo(3, 7);
                                        ctx.moveTo(11, 7); ctx.lineTo(13, 7);
                                        ctx.stroke();
                                    }
                                }
                            }

                            Text {
                                width: 44
                                horizontalAlignment: Text.AlignRight
                                text: Math.round(brightnessSlider.value) + "%"
                                color: "#86868b"
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

                // Row 3: Night Light Toggle
                MacSettingRow {
                    title: "Night Light"
                    subtitle: "Warmer screen colors reduce eye strain at night"
                    showSeparator: true

                    MacSwitch {
                        checked: bridge.nightLight
                        onToggled: (val) => bridge.setNightLight(val)
                    }
                }

                // Row 4: VRR Toggle
                MacSettingRow {
                    title: "Variable Refresh Rate (VRR / Adaptive Sync)"
                    subtitle: "Synchronizes output frame rate to GPU render pipeline"
                    showSeparator: false

                    MacSwitch {
                        checked: bridge.vrrEnabled
                        onToggled: (val) => bridge.setVrrEnabled(val)
                    }
                }
            }
        }

        // Section Title: Hardware Details
        Text {
            text: "HARDWARE DETAILS"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card: Hardware & Compositor ───────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: hwCol.implicitHeight

            ColumnLayout {
                id: hwCol
                anchors.fill: parent
                spacing: 0

                MacSettingRow {
                    title: "Compositor Engine"
                    subtitle: typeof bridge !== "undefined" ? bridge.compositorInfo : "tinexus-comp (wlroots 0.19.2)"
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
