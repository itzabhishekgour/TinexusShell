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

        // Section Title
        Text {
            text: "SYSTEM KEYBOARD SHORTCUTS"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card: Keyboard Shortcuts ──────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: shortCol.implicitHeight

            ColumnLayout {
                id: shortCol
                anchors.fill: parent
                spacing: 0

                property var shortcutsList: [
                    { keys: ["Ctrl", "K"], desc: "Command Palette & Global Search" },
                    { keys: ["Super", "1 – 9"], desc: "Switch to virtual workspace 1 through 9" },
                    { keys: ["Super", "L"], desc: "Lock desktop session immediately" },
                    { keys: ["Super", "← / → / ↑"], desc: "Snap window to left half, right half, or maximize" },
                    { keys: ["Alt", "Tab"], desc: "Interactive window switcher" },
                    { keys: ["Super", "Return"], desc: "Launch default terminal emulator" },
                    { keys: ["Super", "E"], desc: "Launch file manager" },
                    { keys: ["Super", "Q"], desc: "Close focused toplevel window" }
                ]

                Repeater {
                    model: shortCol.shortcutsList
                    delegate: Item {
                        Layout.fillWidth: true
                        height: 44

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16
                            spacing: 14

                            // Keycaps Pill Container
                            Row {
                                spacing: 4
                                Layout.alignment: Qt.AlignVCenter

                                Repeater {
                                    model: modelData.keys
                                    delegate: Rectangle {
                                        width: Math.max(28, keyText.implicitWidth + 12)
                                        height: 22
                                        radius: 4
                                        color: "#323235"
                                        border.color: "#20ffffff"
                                        border.width: 1

                                        Text {
                                            id: keyText
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: "#f5f5f7"
                                            font.pixelSize: 11
                                            font.weight: Font.Medium
                                        }
                                    }
                                }
                            }

                            // Description
                            Text {
                                text: modelData.desc
                                color: "#86868b"
                                font.pixelSize: 12
                                font.weight: Font.Normal
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        // Hairline Separator
                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: "#12ffffff"
                            visible: index < shortCol.shortcutsList.length - 1
                        }
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
