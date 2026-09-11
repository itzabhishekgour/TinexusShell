// ============================================================================
// LauncherWindow.qml — Spotlight-style Command Palette Launcher Window
// Ref: 05_UI_UX_GUIDELINES.md §4.2, §5.4, §5.5, §6.1, §6.4, §10.2
// ============================================================================
import QtQuick
import QtQuick.Window
import "../../common/qml"

Window {
    id: rootWindow

    width: 640
    height: Math.min(480, (searchBox.height + resultsCol.height + 36))
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    onVisibleChanged: {
        if (visible) {
            searchInput.text = ""
            if (typeof bridge !== "undefined") {
                bridge.query = ""
            }
            searchInput.forceActiveFocus()
        }
    }

    // Container with LiquidGlass using explicit launcher glass.background (40px blur)
    Item {
        id: container
        anchors.fill: parent

        // Outer drop shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -8
            radius: 26
            color: Qt.rgba(0, 0, 0, 0.55)
        }

        LiquidGlass {
            id: glassBg
            anchors.fill: parent
            materialType: "launcher"
            cornerRadius: 18
            blurAmount: 40
            fluidOpacity: 0.78
            tintColor: "#14151B"
            fallbackColor: "#14151B"
        }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12

            // ── Search Input Row (Spotlight Style) ──────────────────
            Rectangle {
                id: searchBox
                width: parent.width
                height: 48
                radius: 12
                color: Qt.rgba(1, 1, 1, 0.06)
                border.color: searchInput.activeFocus ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(1, 1, 1, 0.08)
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 12

                    Text {
                        text: "⌕"
                        color: Qt.rgba(1, 1, 1, 0.55)
                        font.pixelSize: 22
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Item {
                        width: parent.width - 80
                        height: parent.height
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            text: "Tinexus Search"
                            color: Qt.rgba(1, 1, 1, 0.35)
                            font.pixelSize: 17
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !searchInput.text
                        }

                        TextInput {
                            id: searchInput
                            anchors.fill: parent
                            verticalAlignment: TextInput.AlignVCenter
                            text: typeof bridge !== "undefined" ? bridge.query : ""
                            color: "#FFFFFF"
                            font.pixelSize: 17
                            focus: true
                            selectByMouse: true

                            onTextChanged: {
                                if (typeof bridge !== "undefined" && bridge.query !== text) {
                                    bridge.query = text;
                                }
                            }

                            Keys.onPressed: function(event) {
                                if (typeof bridge === "undefined") return;

                                if (event.key === Qt.Key_Down) {
                                    bridge.selectNext();
                                    event.accepted = true;
                                } else if (event.key === Qt.Key_Up) {
                                    bridge.selectPrev();
                                    event.accepted = true;
                                } else if (event.key === Qt.Key_Tab) {
                                    bridge.tabComplete();
                                    event.accepted = true;
                                } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                    bridge.launchSelected();
                                    event.accepted = true;
                                } else if (event.key === Qt.Key_Escape) {
                                    bridge.closeLauncher();
                                    event.accepted = true;
                                } else if ((event.modifiers & Qt.ControlModifier) && event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
                                    var idx = event.key - Qt.Key_1;
                                    bridge.launchIndex(idx);
                                    event.accepted = true;
                                }
                            }
                        }
                    }

                    Text {
                        text: "esc"
                        color: Qt.rgba(1, 1, 1, 0.30)
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Divider
            Rectangle {
                width: parent.width
                height: 1
                color: Qt.rgba(1, 1, 1, 0.08)
                visible: typeof bridge !== "undefined" && bridge.hasResults
            }

            // ── Search Results List ─────────────────────────────────
            Column {
                id: resultsCol
                width: parent.width
                spacing: 4

                Repeater {
                    model: typeof bridge !== "undefined" ? bridge.results : []

                    delegate: Rectangle {
                        id: resultRow
                        readonly property bool isSelected: (typeof bridge !== "undefined" && bridge.selectedIndex === index)

                        width: parent.width
                        height: 52
                        radius: 10
                        color: isSelected ? "#0A84FF" : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            // Icon Glyph
                            Rectangle {
                                width: 32
                                height: 32
                                radius: 8
                                anchors.verticalCenter: parent.verticalCenter
                                color: {
                                    if (modelData.kind === "Calculator") return "#FF9F0A";
                                    if (modelData.kind === "Store") return "#30B0C7";
                                    return isSelected ? Qt.rgba(1, 1, 1, 0.25) : "#252834";
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: {
                                        if (modelData.kind === "Calculator") return "=";
                                        if (modelData.kind === "Store") return "🛒";
                                        if (modelData.name.indexOf("Terminal") !== -1) return ">_";
                                        if (modelData.name.indexOf("Files") !== -1) return "📁";
                                        if (modelData.name.indexOf("Settings") !== -1) return "⚙";
                                        if (modelData.name.indexOf("Monitor") !== -1) return "📊";
                                        return "📦";
                                    }
                                    color: "#FFFFFF"
                                    font.bold: true
                                    font.pixelSize: 14
                                }
                            }

                            // Text Column
                            Column {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width - 120
                                spacing: 2

                                Text {
                                    text: modelData.name
                                    color: "#FFFFFF"
                                    font.pixelSize: 14
                                    font.bold: true
                                    elide: Text.ElideRight
                                    width: parent.width
                                }

                                Text {
                                    text: modelData.description
                                    color: isSelected ? Qt.rgba(1, 1, 1, 0.85) : Qt.rgba(1, 1, 1, 0.50)
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    width: parent.width
                                }
                            }

                            // Right Badges (⌘1..9 or Action badge)
                            Item {
                                width: 50
                                height: parent.height

                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: index < 9 ? 24 : 48
                                    height: 20
                                    radius: 4
                                    color: isSelected ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.08)

                                    Text {
                                        anchors.centerIn: parent
                                        text: index < 9 ? ("⌘" + (index + 1)) : "Enter"
                                        color: isSelected ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.60)
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: rowMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    bridge.launchIndex(index);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
