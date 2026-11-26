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

    readonly property int shadowPad: 24
    readonly property int cardWidth: 640
    readonly property int cardHeight: Math.min(540, (searchBox.height + (resultsList.count > 0 ? resultsList.height + 20 : 0) + 36))

    width: cardWidth + shadowPad * 2
    height: cardHeight + shadowPad * 2
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
            bridge.notifyVisibilityChanged(true);
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
        if (typeof bridge !== "undefined") {
            bridge.notifyVisibilityChanged(false);
        }
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
            if (typeof bridge !== "undefined") {
                bridge.notifyVisibilityChanged(false);
                bridge.syncBlurRegion(0, 0, 0, 0, false);
            }
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
        x: rootWindow.shadowPad
        y: rootWindow.shadowPad
        width: rootWindow.cardWidth
        height: rootWindow.cardHeight
        transformOrigin: Item.Top

        property real animOffsetY: isOpen ? 0.0 : -10.0

        // Critically-damped macOS Spotlight drop & scale settle (0.95 -> 1.0)
        scale: isOpen ? 1.0 : 0.95
        opacity: isOpen ? 1.0 : 0.0
        transform: Translate {
            y: container.animOffsetY
        }

        function scheduleBlurSync() {
            Qt.callLater(function() {
                if (typeof bridge === "undefined" || typeof bridge.syncBlurRegion !== "function") return;
                const vis = rootWindow.visible && (container.opacity > 0.05);
                if (!vis) {
                    bridge.syncBlurRegion(0, 0, 0, 0, false);
                    return;
                }
                const sw = Math.max(1, Math.round(container.width * container.scale));
                const sh = Math.max(1, Math.round(container.height * container.scale));
                const sx = Math.round(container.x + (container.width - sw) / 2.0);
                const sy = Math.round(container.y + container.animOffsetY);
                bridge.syncBlurRegion(sx, sy, sw, sh, true);
            });
        }

        onWidthChanged: scheduleBlurSync()
        onHeightChanged: scheduleBlurSync()
        onScaleChanged: scheduleBlurSync()
        onAnimOffsetYChanged: scheduleBlurSync()
        onOpacityChanged: scheduleBlurSync()

        Behavior on scale {
            SpringAnimation {
                spring: 6.2
                damping: 0.86
                epsilon: 0.001
            }
        }

        Behavior on animOffsetY {
            SpringAnimation {
                spring: 6.2
                damping: 0.86
                epsilon: 0.05
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: 145
                easing.type: Easing.OutCubic
            }
        }

        LiquidGlass {
            id: glassBg
            anchors.fill: parent
            materialType: "launcher"
            cornerRadius: 18
            blurAmount: 36
            fluidOpacity: 0.68
            topSpecularOpacity: 0.18
            shadowEnabled: true
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
                                } else if ((event.modifiers & (Qt.ControlModifier | Qt.AltModifier)) && event.key >= Qt.Key_1 && event.key <= Qt.Key_9) {
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
                onCountChanged: container.scheduleBlurSync()

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

                        // Real App Icon (Papirus via image://tinexus/) with crisp fallback
                        Item {
                            width: 34
                            height: 34
                            anchors.verticalCenter: parent.verticalCenter

                            Rectangle {
                                anchors.fill: parent
                                radius: 9
                                visible: !launcherRealIcon.visible
                                color: {
                                    if (modelData.kind === "Calculator") return "#FF9F0A";
                                    if (modelData.kind === "Store") return "#30B0C7";
                                    if (modelData.name.indexOf("Firefox") !== -1) return "#EA580C";
                                    return isSelected ? Qt.rgba(1, 1, 1, 0.25) : "#252834";
                                }
                            }

                            Image {
                                id: launcherRealIcon
                                anchors.centerIn: parent
                                width: 30
                                height: 30
                                source: (modelData.kind !== "Calculator" && modelData.iconUrl) ? modelData.iconUrl : ""
                                sourceSize.width: 64
                                sourceSize.height: 64
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                visible: source.toString().length > 0 && status === Image.Ready
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !launcherRealIcon.visible
                                text: {
                                    if (modelData.kind === "Calculator") return "=";
                                    if (modelData.kind === "Store") return "S";
                                    if (modelData.name.indexOf("Terminal") !== -1) return ">_";
                                    return modelData.name.length > 0 ? modelData.name.substring(0, 1).toUpperCase() : "A";
                                }
                                color: "#FFFFFF"
                                font.family: "Inter"
                                font.bold: true
                                font.pixelSize: 14
                            }
                        }

                        // Text Column
                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 178
                            spacing: 2

                            Text {
                                text: modelData.name
                                color: "#FFFFFF"
                                font.family: "Inter"
                                font.pixelSize: 14
                                font.bold: true
                                elide: Text.ElideRight
                                width: parent.width
                            }

                            Text {
                                text: modelData.description
                                color: isSelected ? Qt.rgba(1, 1, 1, 0.85) : Qt.rgba(1, 1, 1, 0.50)
                                font.family: "Inter"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                width: parent.width
                            }
                        }

                        // Right Badges (Pin button + Alt+1..9 / Enter badge)
                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6

                            Rectangle {
                                id: pinBtn
                                width: 22
                                height: 22
                                radius: 6
                                anchors.verticalCenter: parent.verticalCenter
                                visible: (rowMouse.containsMouse || isSelected) && modelData.kind !== "Calculator" && modelData.kind !== "Store"
                                color: pinMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.28) : Qt.rgba(1, 1, 1, 0.14)
                                border.color: Qt.rgba(1, 1, 1, 0.16)
                                border.width: 1

                                // Crisp geometric pin glyph (avoids missing emoji glyphs)
                                Item {
                                    anchors.centerIn: parent
                                    width: 10
                                    height: 12

                                    Rectangle {
                                        x: 2; y: 1; width: 6; height: 2; radius: 1
                                        color: "#FFFFFF"
                                    }
                                    Rectangle {
                                        x: 3; y: 3; width: 4; height: 4
                                        color: Qt.rgba(1, 1, 1, 0.85)
                                    }
                                    Rectangle {
                                        x: 1; y: 7; width: 8; height: 1.5; radius: 0.75
                                        color: "#FFFFFF"
                                    }
                                    Rectangle {
                                        x: 4.5; y: 8.5; width: 1.2; height: 3; radius: 0.6
                                        color: "#FFFFFF"
                                    }
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
                                anchors.verticalCenter: parent.verticalCenter
                                width: Math.max(index < 9 ? 44 : 58, badgeText.implicitWidth + 12)
                                height: 22
                                radius: 6
                                color: isSelected ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(1, 1, 1, 0.10)
                                border.color: Qt.rgba(1, 1, 1, isSelected ? 0.28 : 0.12)
                                border.width: 1

                                Text {
                                    id: badgeText
                                    anchors.centerIn: parent
                                    text: index < 9 ? ("Alt+" + (index + 1)) : (isSelected ? "↵ Enter" : "Enter")
                                    color: isSelected ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.72)
                                    font.family: "JetBrains Mono"
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
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
