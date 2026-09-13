import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: 240
    color: "#252526" // Subtle contrast against main content #1e1e1e (macOS dark sidebar)

    // Subtle right border separator
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: "#14ffffff"
    }

    property string searchText: ""

    property var navItems: [
        { name: "Wi-Fi", iconId: "wifi", iconColor: "#0A84FF", page: 3 },
        { name: "Sound", iconId: "sound", iconColor: "#FF2D55", page: 1 },
        { name: "Appearance", iconId: "appearance", iconColor: "#AF52DE", page: 2 },
        { name: "Displays", iconId: "display", iconColor: "#0A84FF", page: 0 },
        { name: "Battery & Power", iconId: "power", iconColor: "#30D158", page: 4 },
        { name: "Keyboard", iconId: "keyboard", iconColor: "#636366", page: 5 },
        { name: "Privacy & Security", iconId: "privacy", iconColor: "#34C759", page: 6 },
        { name: "General", iconId: "about", iconColor: "#8E8E93", page: 7 }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 14
        anchors.bottomMargin: 12
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10

        // ── Top Header: Traffic Lights & Drag Area ───────────────────────
        Item {
            Layout.fillWidth: true
            height: 24
            Layout.leftMargin: 4
            Layout.bottomMargin: 2

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    var win = root.Window.window
                    if (win && typeof win.startSystemMove === "function") {
                        win.startSystemMove()
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                spacing: 8

                MacTrafficLights {
                    id: trafficLights
                    targetWindow: root.Window.window
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ── Search Bar (Compact macOS style) ──────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 28
            radius: 6
            color: "#1c1c1e"
            border.color: searchInput.activeFocus ? "#0A84FF" : "#14ffffff"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Canvas {
                    width: 12
                    height: 12
                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.reset();
                        ctx.strokeStyle = "#86868b";
                        ctx.lineWidth = 1.4;
                        ctx.beginPath();
                        ctx.arc(4.5, 4.5, 3.2, 0, 2 * Math.PI);
                        ctx.stroke();
                        ctx.beginPath();
                        ctx.moveTo(7, 7);
                        ctx.lineTo(11, 11);
                        ctx.stroke();
                    }
                }

                TextInput {
                    id: searchInput
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    color: "#f5f5f7"
                    selectByMouse: true
                    clip: true
                    onTextChanged: root.searchText = text

                    Text {
                        anchors.fill: parent
                        text: "Search"
                        color: "#86868b"
                        font.pixelSize: 12
                        visible: !searchInput.text && !searchInput.activeFocus
                    }
                }

                Text {
                    text: "×"
                    color: "#86868b"
                    font.pixelSize: 13
                    font.bold: true
                    visible: searchInput.text.length > 0

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            searchInput.text = ""
                            root.searchText = ""
                        }
                    }
                }
            }
        }

        // ── User Profile Card (macOS Apple Account Style) ─────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 48
            radius: 7
            color: profileArea.containsMouse ? "#0cffffff" : "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 9

                // Circular Avatar
                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "#0A84FF" }
                        GradientStop { position: 1.0; color: "#0062CC" }
                    }
                    border.color: Qt.rgba(1, 1, 1, 0.25)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "AG"
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                ColumnLayout {
                    spacing: 1
                    Layout.fillWidth: true

                    Text {
                        text: "Abhishek Gour"
                        color: "#f5f5f7"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "Tinexus Account"
                        color: "#86868b"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // Update badge dot
                Rectangle {
                    width: 16
                    height: 16
                    radius: 8
                    color: "#ff3b30"
                    visible: true

                    Text {
                        anchors.centerIn: parent
                        text: "1"
                        color: "#ffffff"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            MouseArea {
                id: profileArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: bridge.selectPage(7) // Go to General/About
            }
        }

        // Hairline Divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#12ffffff"
        }

        // ── Navigation Categories List (Tight Spacing, List Feel) ──────────
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 2 // Tight list feel

                Repeater {
                    model: {
                        if (!root.searchText) return root.navItems
                        var filtered = []
                        for (var i = 0; i < root.navItems.length; ++i) {
                            if (root.navItems[i].name.toLowerCase().indexOf(root.searchText.toLowerCase()) !== -1) {
                                filtered.push(root.navItems[i])
                            }
                        }
                        return filtered
                    }

                    delegate: Rectangle {
                        id: itemDelegate
                        Layout.fillWidth: true
                        height: 30
                        radius: 6

                        property bool isSelected: bridge.currentPage === modelData.page

                        // Selected: soft macOS blue gradient (#0A84FF to #0071E3)
                        gradient: isSelected ? selectedGrad : null
                        color: !isSelected && itemArea.containsMouse ? "#0effffff" : "transparent"

                        Gradient {
                            id: selectedGrad
                            GradientStop { position: 0.0; color: "#0A84FF" }
                            GradientStop { position: 1.0; color: "#0071E3" }
                        }

                        border.color: isSelected ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                        border.width: isSelected ? 1 : 0

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            spacing: 8

                            MacSquircleIcon {
                                iconSize: 20
                                glyphSize: 11
                                iconColor: modelData.iconColor
                                iconId: modelData.iconId ? modelData.iconId : ""
                                iconText: modelData.iconText ? modelData.iconText : ""
                            }

                            Text {
                                text: modelData.name
                                color: itemDelegate.isSelected ? "#ffffff" : "#e5e5ea"
                                font.pixelSize: 13
                                font.weight: itemDelegate.isSelected ? Font.Medium : Font.Normal
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: itemArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: bridge.selectPage(modelData.page)
                        }
                    }
                }
            }
        }

        // ── Bottom Platform Version ────────────────────────────────────────
        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Tinexus OS v" + bridge.platformVersion
            color: "#636366"
            font.pixelSize: 11
        }
    }
}
