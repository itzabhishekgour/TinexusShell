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

        // Section Title: System Information
        Text {
            text: "SYSTEM INFORMATION"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card: About Tinexus Specs ─────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: aboutCol.implicitHeight

            ColumnLayout {
                id: aboutCol
                anchors.fill: parent
                spacing: 0

                // Header Banner inside card
                Item {
                    Layout.fillWidth: true
                    height: 64

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 14

                        MacSquircleIcon {
                            iconSize: 42
                            glyphSize: 22
                            iconColor: "#0A84FF"
                            iconText: "T"
                        }

                        ColumnLayout {
                            spacing: 1
                            Layout.fillWidth: true

                            Text {
                                text: "Tinexus Desktop Platform"
                                color: "#f5f5f7"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            Text {
                                text: "Version " + bridge.platformVersion + " (Release Build)"
                                color: "#86868b"
                                font.pixelSize: 11
                            }
                        }

                        MacButton {
                            text: "Save Config"
                            primary: true
                            onClicked: bridge.saveConfig()
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

                // Specs Rows with hairline dividers
                MacSettingRow {
                    title: "Operating System"
                    showSeparator: true

                    Text {
                        text: typeof bridge !== "undefined" ? bridge.osVersion : "Tinexus Linux (x86_64)"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }

                MacSettingRow {
                    title: "Processor"
                    showSeparator: true

                    Text {
                        text: typeof bridge !== "undefined" ? bridge.cpuModel : "Processor"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }

                MacSettingRow {
                    title: "Memory (RAM)"
                    showSeparator: true

                    Text {
                        text: typeof bridge !== "undefined" ? bridge.memInfo : "RAM"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }

                MacSettingRow {
                    title: "System Storage"
                    showSeparator: true

                    Text {
                        text: typeof bridge !== "undefined" ? bridge.storageInfo : "Storage"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }

                MacSettingRow {
                    title: "Display"
                    showSeparator: true

                    Text {
                        text: typeof bridge !== "undefined" ? bridge.displayInfo : "Display"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }

                MacSettingRow {
                    title: "Wayland Compositor"
                    showSeparator: true

                    Text {
                        text: typeof bridge !== "undefined" ? bridge.compositorInfo : "tinexus-comp"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }

                MacSettingRow {
                    title: "Graphics Engine"
                    showSeparator: true

                    Text {
                        text: typeof bridge !== "undefined" ? bridge.graphicsEngine : "Graphics Engine"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }

                MacSettingRow {
                    title: "IPC Architecture"
                    showSeparator: true

                    Text {
                        text: "D-Bus Architecture (sd-bus / io.tinexus.shell.*)"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }

                MacSettingRow {
                    title: "License"
                    showSeparator: false

                    Text {
                        text: "GPL-2.0 / Apache-2.0"
                        color: "#86868b"
                        font.pixelSize: 12
                    }
                }
            }
        }

        // Section Title: Software Update
        Text {
            text: "SOFTWARE UPDATE"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card: Software Update ─────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: updateCol.implicitHeight

            ColumnLayout {
                id: updateCol
                anchors.fill: parent
                spacing: 0

                MacSettingRow {
                    title: "Software Update"
                    subtitle: "Tinexus Platform v" + bridge.platformVersion + " is up to date"
                    showSeparator: false

                    MacButton {
                        text: "Check for Updates"
                        onClicked: bridge.checkForUpdates()
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
