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

        // Section Title: Power & Sleep
        Text {
            text: "POWER & SLEEP"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card 1: Power Controls ────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: powerCol.implicitHeight

            ColumnLayout {
                id: powerCol
                anchors.fill: parent
                spacing: 0

                // Row 1: Energy Mode
                MacSettingRow {
                    title: "Energy Mode"
                    subtitle: "Optimize for battery life or high performance"
                    showSeparator: true

                    MacComboBox {
                        model: ["Power Saver", "Balanced", "Performance"]
                        currentIndex: bridge.powerProfileIndex
                        onActivated: (index) => bridge.setPowerProfileIndex(index)
                    }
                }

                // Row 2: Display timeout inline slider
                Item {
                    Layout.fillWidth: true
                    height: 48

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        Text {
                            text: "Turn display off on battery when inactive"
                            color: "#f5f5f7"
                            font.pixelSize: 13
                            font.weight: Font.Normal
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            MacSlider {
                                id: screenTimeoutSlider
                                width: 220
                                from: 1
                                to: 60
                                stepSize: 1
                                value: bridge.screenTimeoutMin
                                onMoved: (val) => bridge.setScreenTimeoutMin(Math.round(val))
                            }

                            Text {
                                width: 48
                                horizontalAlignment: Text.AlignRight
                                text: Math.round(screenTimeoutSlider.value) + " min"
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

                // Row 3: System sleep inline slider
                Item {
                    Layout.fillWidth: true
                    height: 48

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12

                        Text {
                            text: "Put computer to sleep when inactive"
                            color: "#f5f5f7"
                            font.pixelSize: 13
                            font.weight: Font.Normal
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            MacSlider {
                                id: sleepSlider
                                width: 220
                                from: 5
                                to: 120
                                stepSize: 5
                                value: bridge.sleepAfterMin
                                onMoved: (val) => bridge.setSleepAfterMin(Math.round(val))
                            }

                            Text {
                                width: 48
                                horizontalAlignment: Text.AlignRight
                                text: Math.round(sleepSlider.value) + " min"
                                color: "#86868b"
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }

        // Section Title: Lock Screen
        Text {
            text: "LOCK SCREEN & SECURITY"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card 2: Lock Screen ───────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: lockCol.implicitHeight

            ColumnLayout {
                id: lockCol
                anchors.fill: parent
                spacing: 0

                MacSettingRow {
                    title: "Require password after display is turned off"
                    showSeparator: true

                    MacSwitch {
                        checked: bridge.lockOnSleep
                        onToggled: (val) => bridge.setLockOnSleep(val)
                    }
                }

                MacSettingRow {
                    title: "PAM User Authentication"
                    subtitle: "Verify lock screen unlock against local Linux credentials"
                    showSeparator: false

                    MacSwitch {
                        checked: bridge.pamAuth
                        onToggled: (val) => bridge.setPamAuth(val)
                    }
                }
            }
        }

        // Section Title: Clipboard
        Text {
            text: "CLIPBOARD HISTORY"
            color: "#86868b"
            font.pixelSize: 11
            font.weight: Font.Medium
            Layout.leftMargin: 6
        }

        // ── Grouped Card 3: Clipboard ─────────────────────────────────────
        MacCard {
            Layout.fillWidth: true
            implicitHeight: clipCol.implicitHeight

            ColumnLayout {
                id: clipCol
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
                            text: "Clipboard History Capacity"
                            color: "#f5f5f7"
                            font.pixelSize: 13
                            font.weight: Font.Normal
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            spacing: 8
                            Layout.alignment: Qt.AlignVCenter

                            MacSlider {
                                id: clipSlider
                                width: 220
                                from: 10
                                to: 200
                                stepSize: 10
                                value: bridge.clipboardHistorySize
                                onMoved: (val) => bridge.setClipboardHistorySize(Math.round(val))
                            }

                            Text {
                                width: 56
                                horizontalAlignment: Text.AlignRight
                                text: Math.round(clipSlider.value) + " items"
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
                    title: "Clear All Clipboard History"
                    subtitle: "Permanently erase stored clips from memory and disk"
                    showSeparator: false

                    MacButton {
                        text: "Clear Now"
                        destructive: true
                        onClicked: bridge.clearClipboardHistory()
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
