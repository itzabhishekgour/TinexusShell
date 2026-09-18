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

        // Section Title: Appearance
        Text {
            text: "APPEARANCE"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card 1: Theme Mode (Light / Dark / Auto) ─────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: themeCol.implicitHeight + 28

            ColumnLayout {
                id: themeCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12

                RowLayout {
                    spacing: 24
                    Layout.alignment: Qt.AlignHCenter

                    // Light Mode Preview
                    ColumnLayout {
                        spacing: 6
                        Rectangle {
                            width: 88
                            height: 56
                            radius: 8
                            color: "#e5e5ea"
                            border.color: bridge.themeMode === "Light" ? "#0A84FF" : "#20ffffff"
                            border.width: bridge.themeMode === "Light" ? 2 : 1

                            Rectangle {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 12
                                radius: 8
                                color: "#d1d1d6"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.setThemeMode("Light")
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Light"
                            color: bridge.themeMode === "Light" ? "#f5f5f7" : "#86868b"
                            font.pixelSize: 12
                            font.weight: bridge.themeMode === "Light" ? Font.Medium : Font.Normal
                        }
                    }

                    // Dark Mode Preview
                    ColumnLayout {
                        spacing: 6
                        Rectangle {
                            width: 88
                            height: 56
                            radius: 8
                            color: "#1c1c1e"
                            border.color: bridge.themeMode === "Dark" ? "#0A84FF" : "#20ffffff"
                            border.width: bridge.themeMode === "Dark" ? 2 : 1

                            Rectangle {
                                anchors.top: parent.top
                                anchors.left: parent.left
                                anchors.right: parent.right
                                height: 12
                                radius: 8
                                color: "#28282a"
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.setThemeMode("Dark")
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Dark"
                            color: bridge.themeMode === "Dark" ? "#f5f5f7" : "#86868b"
                            font.pixelSize: 12
                            font.weight: bridge.themeMode === "Dark" ? Font.Medium : Font.Normal
                        }
                    }

                    // Auto Mode Preview
                    ColumnLayout {
                        spacing: 6
                        Rectangle {
                            width: 88
                            height: 56
                            radius: 8
                            border.color: bridge.themeMode === "Auto" ? "#0A84FF" : "#20ffffff"
                            border.width: bridge.themeMode === "Auto" ? 2 : 1
                            clip: true

                            Row {
                                anchors.fill: parent
                                Rectangle { width: 44; height: 56; color: "#e5e5ea" }
                                Rectangle { width: 44; height: 56; color: "#1c1c1e" }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.setThemeMode("Auto")
                            }
                        }

                        Text {
                            Layout.alignment: Qt.AlignHCenter
                            text: "Auto"
                            color: bridge.themeMode === "Auto" ? "#f5f5f7" : "#86868b"
                            font.pixelSize: 12
                            font.weight: bridge.themeMode === "Auto" ? Font.Medium : Font.Normal
                        }
                    }
                }
            }
        }

        // Section Title: Accent Color
        Text {
            text: "ACCENT COLOR"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card 2: Accent Color Palette ──────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: accentCol.implicitHeight + 28

            ColumnLayout {
                id: accentCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    spacing: 14
                    Layout.alignment: Qt.AlignHCenter

                    property var palette: [
                        { color: "#00c3ff", name: "Electric Cyan" },
                        { color: "#0A84FF", name: "macOS Blue" },
                        { color: "#5856d6", name: "Deep Violet" },
                        { color: "#ff2d55", name: "Hot Pink" },
                        { color: "#ff9500", name: "Vibrant Orange" },
                        { color: "#30d158", name: "Mint Green" }
                    ]

                    Repeater {
                        model: parent.palette
                        delegate: Rectangle {
                            width: 28
                            height: 28
                            radius: 14
                            color: modelData.color
                            border.color: bridge.accentIndex === index ? "#ffffff" : "transparent"
                            border.width: 2

                            Rectangle {
                                anchors.centerIn: parent
                                width: 8
                                height: 8
                                radius: 4
                                color: "#ffffff"
                                visible: bridge.accentIndex === index
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: bridge.setAccentIndex(index)
                            }
                        }
                    }
                }
            }
        }

        // Section Title: Wallpaper
        Text {
            text: "WALLPAPER"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card 3: Wallpaper Picker ──────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: wallCol.implicitHeight + 28

            ColumnLayout {
                id: wallCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Repeater {
                        model: typeof bridge !== "undefined" ? bridge.wallpapers : []

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 70
                            radius: 6
                            readonly property bool isSelected: typeof bridge !== "undefined" && bridge.wallpaperIndex === index
                            border.color: isSelected ? "#0A84FF" : "#1affffff"
                            border.width: isSelected ? 2 : 1
                            color: modelData.previewColor || "#18223c"
                            clip: true

                            Image {
                                anchors.fill: parent
                                source: modelData.path ? ("file://" + modelData.path) : ""
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: true
                            }

                            // Bottom gradient scrim for text readability
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 26
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "transparent" }
                                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.75) }
                                }
                            }

                            Text {
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.name
                                color: "#ffffff"
                                font.pixelSize: 11
                                font.weight: isSelected ? Font.Bold : Font.Medium
                                elide: Text.ElideRight
                                width: parent.width - 8
                                horizontalAlignment: Text.AlignHCenter
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (typeof bridge !== "undefined") {
                                        bridge.setSelectedWallpaperIndex(index);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
