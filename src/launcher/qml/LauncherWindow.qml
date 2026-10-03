// ============================================================================
// LauncherWindow.qml — Spotlight-style Command Palette Launcher Window
// Ref: 05_UI_UX_GUIDELINES.md §4.2, §5.4, §5.5, §6.1, §6.4, §10.2
// ============================================================================
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import "../../common/qml"

Window {
    id: rootWindow

    width: 640
    height: Math.min(540, (searchBox.height + (resultsList.count > 0 ? resultsList.height + 20 : 0) + 36) + 24)
    color: "transparent"
    flags: Qt.FramelessWindowHint

    visible: false

    // "Drop from Notch" Animation State & Focus Tracking
    property bool isOpen: visible
    property bool hasBeenActive: false
    property bool canDismiss: false

    Timer {
        id: activationGraceTimer
        interval: 350
        repeat: false
        onTriggered: {
            rootWindow.canDismiss = true;
        }
    }

    function openLauncher() {
        isOpen = true;
        visible = true;
        hasBeenActive = false;
        canDismiss = false;
        dismissTimer.stop();
        activationGraceTimer.restart();
        searchInput.text = "";
        if (typeof bridge !== "undefined") {
            bridge.query = "";
        }
        Qt.callLater(function() {
            rootWindow.requestActivate();
            searchInput.forceActiveFocus();
        });
    }

    function dismissLauncher() {
        if (!isOpen && !visible) return;
        isOpen = false;
        hasBeenActive = false;
        canDismiss = false;
        activationGraceTimer.stop();
        dismissTimer.restart();
    }

    Timer {
        id: dismissTimer
        interval: 220
        repeat: false
        onTriggered: {
            rootWindow.visible = false;
            // Clear query silently without emitting closeRequested
            if (typeof bridge !== "undefined") {
                bridge.query = "";
            }
        }
    }

    onVisibleChanged: {
        if (visible) {
            openLauncher();
        } else {
            isOpen = false;
            hasBeenActive = false;
            canDismiss = false;
            activationGraceTimer.stop();
            dismissTimer.stop();
        }
    }

    onActiveChanged: {
        if (active) {
            hasBeenActive = true;
        } else if (visible && hasBeenActive && canDismiss) {
            dismissLauncher();
        }
    }

    Connections {
        target: typeof bridge !== "undefined" ? bridge : null
        function onCloseRequested() {
            if (rootWindow.visible && rootWindow.isOpen) {
                rootWindow.dismissLauncher();
            }
        }
    }

    Component.onCompleted: {
        if (visible) {
            openLauncher();
        }
    }

    // ── Drop-from-Notch Container with LiquidGlass & Spring Physics ──────
    Item {
        id: container
        width: parent.width
        height: parent.height - 24
        transformOrigin: Item.Top

        // Physical drop from notch: detaches from notch tip (y: -48) and falls down into workspace (y: 12)
        // while expanding horizontally and vertically (scale: 0.25 -> 1.0)
        scale: isOpen ? 1.0 : 0.25
        y: isOpen ? 12 : -48
        opacity: isOpen ? 1.0 : 0.0

        Behavior on scale {
            SpringAnimation {
                spring: 3.6
                damping: 0.28
                epsilon: 0.005
            }
        }

        Behavior on y {
            SpringAnimation {
                spring: 3.6
                damping: 0.28
                epsilon: 0.005
            }
        }

        Behavior on opacity {
            SpringAnimation {
                spring: 3.6
                damping: 0.28
                epsilon: 0.005
            }
        }

        // Outer specular drop shadow
        Rectangle {
            anchors.fill: parent
            anchors.margins: -10
            radius: 28
            color: Qt.rgba(0, 0, 0, 0.50)
        }

        LiquidGlass {
            id: glassBg
            anchors.fill: parent
            materialType: "launcher"
            cornerRadius: 20
            blurAmount: 40
            fluidOpacity: 0.82
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

                MouseArea {
                    anchors.fill: parent
                    z: -1
                    onClicked: searchInput.forceActiveFocus()
                }

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
                            color: "#FFFFFF"
                            font.pixelSize: 17
                            focus: true
                            selectByMouse: true
                            activeFocusOnTab: true

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
                                } else if (event.key === Qt.Key_Escape || ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_K)) {
                                    rootWindow.dismissLauncher();
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

            // ── Search Results List (Scrollable ListView) ───────────
            ListView {
                id: resultsList
                width: parent.width
                height: Math.min(380, count * 56)
                clip: true
                spacing: 4
                model: typeof bridge !== "undefined" ? bridge.results : []
                currentIndex: typeof bridge !== "undefined" ? bridge.selectedIndex : 0

                ScrollBar.vertical: ScrollBar {
                    policy: resultsList.contentHeight > resultsList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                }

                Connections {
                    target: typeof bridge !== "undefined" ? bridge : null
                    function onSelectedIndexChanged() {
                        if (typeof bridge !== "undefined") {
                            resultsList.positionViewAtIndex(bridge.selectedIndex, ListView.Contain)
                        }
                    }
                    function onQueryChanged() {
                        if (typeof bridge !== "undefined" && searchInput.text !== bridge.query) {
                            searchInput.text = bridge.query
                        }
                    }
                }

                delegate: Rectangle {
                    id: resultRow
                    readonly property bool isSelected: (typeof bridge !== "undefined" && bridge.selectedIndex === index)

                    width: resultsList.width
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
                                if (modelData.name.indexOf("Firefox") !== -1) return "#EA580C";
                                return isSelected ? Qt.rgba(1, 1, 1, 0.25) : "#252834";
                            }

                            Text {
                                anchors.centerIn: parent
                                text: {
                                    if (modelData.kind === "Calculator") return "=";
                                    if (modelData.kind === "Store") return "🛒";
                                    if (modelData.name.indexOf("Firefox") !== -1) return "🌐";
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

                        // Right Badges (Pin button + ⌘1..9/Enter badge)
                        Item {
                            width: 80
                            height: parent.height

                            Rectangle {
                                id: pinBtn
                                anchors.right: badgeRect.left
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                width: 24
                                height: 20
                                radius: 4
                                visible: (rowMouse.containsMouse || isSelected) && modelData.kind !== "Calculator" && modelData.kind !== "Store"
                                color: pinMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.32) : Qt.rgba(1, 1, 1, 0.14)

                                Text {
                                    anchors.centerIn: parent
                                    text: "📌"
                                    font.pixelSize: 11
                                }

                                MouseArea {
                                    id: pinMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (typeof bridge !== "undefined") {
                                            bridge.pinToDock(index);
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                id: badgeRect
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
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        cursorShape: Qt.PointingHandCursor
                        onClicked: function(mouse) {
                            if (mouse.button === Qt.RightButton) {
                                if (typeof bridge !== "undefined") {
                                    bridge.pinToDock(index);
                                }
                            } else {
                                if (typeof bridge !== "undefined") {
                                    bridge.launchIndex(index);
                                    rootWindow.dismissLauncher();
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
