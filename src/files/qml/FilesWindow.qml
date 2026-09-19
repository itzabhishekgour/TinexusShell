// ============================================================================
// FilesWindow.qml — High-fidelity Qt6 / QML File Manager for Tinexus Platform
// 4 View Modes: Icons, List, Columns (with Inspector Pane), Gallery
// Context Menu: Open, Copy, Cut, Paste, Rename, Delete
// ============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../../common/qml"

Window {
    id: rootWindow
    width: 860
    height: 540
    minimumWidth: 720
    minimumHeight: 450
    color: "#181B24"
    flags: Qt.Window | Qt.FramelessWindowHint
    visibility: Window.Windowed
    title: typeof bridge !== "undefined" ? ("Files — " + bridge.currentDirName) : "Files"

    // Context menu states
    property bool contextMenuVisible: false
    property real contextMenuX: 0
    property real contextMenuY: 0
    property string contextMenuTarget: ""
    property bool contextMenuIsDir: false
    property bool contextMenuIsBg: false

    // Modal dialog states
    property bool newFolderDialogVisible: false
    property bool renameDialogVisible: false
    property string renameTarget: ""

    // Phase 3: Interactive Address Bar & Search states
    property bool addressBarEditMode: false

    // Phase 3 Shortcuts: Ctrl+F (Search), Ctrl+L (Address bar), Escape (Cancel search/edit)
    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            searchInput.forceActiveFocus();
            searchInput.selectAll();
        }
    }

    Shortcut {
        sequence: "Ctrl+L"
        onActivated: {
            rootWindow.addressBarEditMode = true;
            addressInput.text = typeof bridge !== "undefined" ? bridge.currentPath : "/";
            addressInput.forceActiveFocus();
            addressInput.selectAll();
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: (typeof bridge !== "undefined" && bridge.isSearching) || rootWindow.addressBarEditMode
        onActivated: {
            rootWindow.addressBarEditMode = false;
            searchInput.text = "";
            searchInput.focus = false;
            if (typeof bridge !== "undefined") {
                bridge.clearSearch();
            }
        }
    }

    // Phase 4 Shortcuts: Multi-Tab Navigation
    Shortcut {
        sequence: "Ctrl+T"
        onActivated: {
            if (typeof bridge !== "undefined") {
                bridge.createTab();
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+W"
        onActivated: {
            if (typeof bridge !== "undefined") {
                bridge.closeTab(bridge.activeTabIndex);
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+Tab"
        onActivated: {
            if (typeof bridge !== "undefined" && bridge.tabCount > 1) {
                bridge.switchTab((bridge.activeTabIndex + 1) % bridge.tabCount);
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        onActivated: {
            if (typeof bridge !== "undefined" && bridge.tabCount > 1) {
                bridge.switchTab((bridge.activeTabIndex - 1 + bridge.tabCount) % bridge.tabCount);
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+Backtab"
        onActivated: {
            if (typeof bridge !== "undefined" && bridge.tabCount > 1) {
                bridge.switchTab((bridge.activeTabIndex - 1 + bridge.tabCount) % bridge.tabCount);
            }
        }
    }

    Shortcut { sequence: "Alt+1"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 0) bridge.switchTab(0); }
    Shortcut { sequence: "Alt+2"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 1) bridge.switchTab(1); }
    Shortcut { sequence: "Alt+3"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 2) bridge.switchTab(2); }
    Shortcut { sequence: "Alt+4"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 3) bridge.switchTab(3); }
    Shortcut { sequence: "Alt+5"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 4) bridge.switchTab(4); }
    Shortcut { sequence: "Alt+6"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 5) bridge.switchTab(5); }
    Shortcut { sequence: "Alt+7"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 6) bridge.switchTab(6); }
    Shortcut { sequence: "Alt+8"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 7) bridge.switchTab(7); }
    Shortcut { sequence: "Alt+9"; onActivated: if (typeof bridge !== "undefined" && bridge.tabCount > 8) bridge.switchTab(8); }

    // Phase 7 Keyboard Shortcuts: Direct execution via existing bridge methods
    readonly property bool isInputFocused: (typeof addressInput !== "undefined" && addressInput.activeFocus) ||
                                           (typeof searchInput !== "undefined" && searchInput.activeFocus) ||
                                           (typeof renameInput !== "undefined" && renameInput.activeFocus) ||
                                           (typeof newFolderInput !== "undefined" && newFolderInput.activeFocus)

    Shortcut {
        sequence: "Ctrl+A"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.selectAll();
        }
    }

    Shortcut {
        sequence: "Ctrl+C"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.copySelected();
        }
    }

    Shortcut {
        sequence: "Ctrl+X"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.cutSelected();
        }
    }

    Shortcut {
        sequence: "Ctrl+V"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.pasteItem();
        }
    }

    Shortcut {
        sequence: "Delete"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.deleteSelected();
        }
    }

    Shortcut {
        sequence: "F2"
        enabled: !rootWindow.isInputFocused && typeof bridge !== "undefined" && bridge.selectedCount === 1
        onActivated: {
            if (typeof bridge !== "undefined" && bridge.selectedPath !== "") {
                rootWindow.renameTarget = bridge.selectedPath;
                rootWindow.renameDialogVisible = true;
            }
        }
    }

    Shortcut {
        sequence: "Return"
        enabled: !rootWindow.isInputFocused && typeof bridge !== "undefined" && bridge.selectedCount === 1
        onActivated: {
            if (typeof bridge !== "undefined" && bridge.selectedPath !== "") {
                var details = bridge.selectedItemDetails;
                bridge.openItem(bridge.selectedPath, details.isDir === true);
            }
        }
    }

    Shortcut {
        sequence: "Enter"
        enabled: !rootWindow.isInputFocused && typeof bridge !== "undefined" && bridge.selectedCount === 1
        onActivated: {
            if (typeof bridge !== "undefined" && bridge.selectedPath !== "") {
                var details = bridge.selectedItemDetails;
                bridge.openItem(bridge.selectedPath, details.isDir === true);
            }
        }
    }

    Shortcut {
        sequence: "Backspace"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.goUp();
        }
    }

    Shortcut {
        sequence: "Alt+Up"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.goUp();
        }
    }

    Shortcut {
        sequence: "Alt+Left"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.goBack();
        }
    }

    Shortcut {
        sequence: "Alt+Right"
        enabled: !rootWindow.isInputFocused
        onActivated: {
            if (typeof bridge !== "undefined") bridge.goForward();
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── 1. Top Window Bar & Toolbar (macOS Finder Style) ────────
        Rectangle {
            Layout.fillWidth: true
            height: 52
            color: "#1E222E"
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            z: 20

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12

                // Traffic Light Window Controls
                MacTrafficLights {
                    targetWindow: rootWindow
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { width: 8 }

                // Navigation: Back / Forward / Up
                Row {
                    spacing: 4
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        width: 28; height: 28; radius: 6
                        color: (typeof bridge !== "undefined" && bridge.canGoBack) ? (bMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.1)) : Qt.rgba(1,1,1,0.04)
                        TinexusVectorIcon {
                            anchors.centerIn: parent
                            name: "chevron-left"
                            size: 12
                            color: (typeof bridge !== "undefined" && bridge.canGoBack) ? "#FFFFFF" : Qt.rgba(1,1,1,0.3)
                        }
                        MouseArea {
                            id: bMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            enabled: typeof bridge !== "undefined" && bridge.canGoBack
                            onClicked: bridge.goBack()
                        }
                    }

                    Rectangle {
                        width: 28; height: 28; radius: 6
                        color: (typeof bridge !== "undefined" && bridge.canGoForward) ? (fMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.1)) : Qt.rgba(1,1,1,0.04)
                        TinexusVectorIcon {
                            anchors.centerIn: parent
                            name: "chevron-right"
                            size: 12
                            color: (typeof bridge !== "undefined" && bridge.canGoForward) ? "#FFFFFF" : Qt.rgba(1,1,1,0.3)
                        }
                        MouseArea {
                            id: fMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            enabled: typeof bridge !== "undefined" && bridge.canGoForward
                            onClicked: bridge.goForward()
                        }
                    }

                    Rectangle {
                        width: 28; height: 28; radius: 6
                        color: uMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.1)
                        TinexusVectorIcon {
                            anchors.centerIn: parent
                            name: "arrow-up"
                            size: 12
                            color: "#FFFFFF"
                        }
                        MouseArea {
                            id: uMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            onClicked: { if (typeof bridge !== "undefined") bridge.goUp() }
                        }
                    }
                }

                // 4-Segment View Mode Switcher [ Icons | List | Columns | Gallery ]
                Rectangle {
                    height: 28
                    width: 144
                    radius: 7
                    color: Qt.rgba(0, 0, 0, 0.40)
                    border.color: Qt.rgba(1, 1, 1, 0.12)
                    border.width: 1
                    Layout.alignment: Qt.AlignVCenter

                    Row {
                        anchors.fill: parent

                        Repeater {
                            model: [
                                { iconName: "view-icons", mode: 0, tip: "Icons" },
                                { iconName: "view-list", mode: 1, tip: "List" },
                                { iconName: "view-columns", mode: 2, tip: "Columns" },
                                { iconName: "view-gallery", mode: 3, tip: "Gallery" }
                            ]

                            delegate: Rectangle {
                                width: 36
                                height: 28
                                radius: 6
                                readonly property bool isActive: typeof bridge !== "undefined" && bridge.currentViewMode === modelData.mode
                                color: isActive ? "#0A84FF" : (segMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.10) : "transparent")

                                TinexusVectorIcon {
                                    anchors.centerIn: parent
                                    name: modelData.iconName
                                    size: 14
                                    color: isActive ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.75)
                                }

                                MouseArea {
                                    id: segMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (typeof bridge !== "undefined") {
                                            bridge.currentViewMode = modelData.mode;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // New Folder Button (+)
                Rectangle {
                    width: 28
                    height: 28
                    radius: 6
                    color: newFMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(1, 1, 1, 0.08)
                    border.color: Qt.rgba(1, 1, 1, 0.12)
                    border.width: 1
                    Layout.alignment: Qt.AlignVCenter

                    TinexusVectorIcon {
                        anchors.centerIn: parent
                        name: "plus"
                        size: 12
                        color: "#FFFFFF"
                    }

                    MouseArea {
                        id: newFMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: rootWindow.newFolderDialogVisible = true
                    }
                }

                // Path Breadcrumb & Editable Address Bar (Ctrl+L)
                Rectangle {
                    Layout.fillWidth: true
                    height: 28
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.35)
                    border.color: rootWindow.addressBarEditMode ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.10)
                    border.width: rootWindow.addressBarEditMode ? 1.5 : 1
                    Layout.alignment: Qt.AlignVCenter

                    // Breadcrumb Display Mode
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8
                        visible: !rootWindow.addressBarEditMode

                        FileVectorIcon {
                            iconKind: "folder"
                            isDir: true
                            iconSize: 16
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Text {
                            text: typeof bridge !== "undefined" ? bridge.currentPath : "/"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Item {
                            width: 16
                            height: 16
                            Layout.alignment: Qt.AlignVCenter
                            TinexusVectorIcon {
                                anchors.centerIn: parent
                                name: "refresh"
                                size: 13
                                color: refMouse.containsMouse ? "#3B82F6" : Qt.rgba(1, 1, 1, 0.6)
                            }
                            MouseArea {
                                id: refMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (typeof bridge !== "undefined") bridge.refresh() }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.IBeamCursor
                            z: -1
                            onClicked: {
                                rootWindow.addressBarEditMode = true;
                                addressInput.text = typeof bridge !== "undefined" ? bridge.currentPath : "/";
                                addressInput.forceActiveFocus();
                                addressInput.selectAll();
                            }
                        }
                    }

                    // Editable Address Input Mode
                    TextInput {
                        id: addressInput
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        visible: rootWindow.addressBarEditMode
                        color: "#FFFFFF"
                        font.pixelSize: 12
                        verticalAlignment: TextInput.AlignVCenter
                        selectByMouse: true

                        onVisibleChanged: {
                            if (visible) {
                                forceActiveFocus();
                                selectAll();
                            }
                        }

                        Keys.onReturnPressed: commitAddress()
                        Keys.onEnterPressed: commitAddress()
                        Keys.onEscapePressed: { rootWindow.addressBarEditMode = false; }

                        function commitAddress() {
                            if (typeof bridge !== "undefined") {
                                var ok = bridge.navigateToPath(text);
                                if (ok) {
                                    rootWindow.addressBarEditMode = false;
                                }
                            }
                        }
                    }
                }

                // Toolbar Search Bar (Ctrl+F)
                Rectangle {
                    id: searchBarBox
                    width: searchInput.activeFocus || (searchInput.text.length > 0) ? 190 : 140
                    height: 28
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.35)
                    border.color: searchInput.activeFocus ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.10)
                    border.width: searchInput.activeFocus ? 1.5 : 1
                    Layout.alignment: Qt.AlignVCenter

                    Behavior on width {
                        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        TinexusVectorIcon {
                            name: "search"
                            size: 12
                            color: Qt.rgba(1, 1, 1, 0.5)
                            Layout.alignment: Qt.AlignVCenter
                        }

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            selectByMouse: true

                            Text {
                                text: typeof bridge !== "undefined" ? ("Search " + bridge.currentDirName) : "Search"
                                color: Qt.rgba(1, 1, 1, 0.35)
                                font.pixelSize: 12
                                anchors.fill: parent
                                verticalAlignment: Text.AlignVCenter
                                visible: !searchInput.text && !searchInput.activeFocus
                            }

                            onTextChanged: {
                                if (typeof bridge !== "undefined") {
                                    bridge.startSearch(text, true);
                                }
                            }

                            Keys.onEscapePressed: {
                                searchInput.text = "";
                                searchInput.focus = false;
                                if (typeof bridge !== "undefined") {
                                    bridge.clearSearch();
                                }
                            }
                        }

                        Rectangle {
                            width: 16
                            height: 16
                            radius: 8
                            color: "transparent"
                            visible: searchInput.text.length > 0
                            Layout.alignment: Qt.AlignVCenter

                            TinexusVectorIcon {
                                anchors.centerIn: parent
                                name: "close"
                                size: 9
                                color: clearMouse.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.5)
                            }

                            MouseArea {
                                id: clearMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    searchInput.text = "";
                                    if (typeof bridge !== "undefined") {
                                        bridge.clearSearch();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── 1A. Multi-Tab Bar (Phase 4: Multi-Tab Navigation) ───────
        Rectangle {
            id: tabBarContainer
            Layout.fillWidth: true
            height: 36
            color: "#161922"
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            z: 21

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                // Scrollable Tabs ListView
                ListView {
                    id: tabsListView
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    Layout.alignment: Qt.AlignVCenter
                    orientation: ListView.Horizontal
                    spacing: 4
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: typeof bridge !== "undefined" ? bridge.tabs : []

                    delegate: Rectangle {
                        id: tabDelegate
                        height: 28
                        width: Math.max(130, Math.min(220, (tabBarContainer.width - 60) / Math.max(1, (typeof bridge !== "undefined" ? bridge.tabCount : 1))))
                        radius: 6

                        readonly property bool isTabActive: modelData.isActive === true

                        color: isTabActive ? "#232836" : (tabMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                        border.color: isTabActive ? Qt.rgba(0.04, 0.52, 1.0, 0.5) : (tabMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.10) : "transparent")
                        border.width: 1

                        // Active indicator accent bar at the bottom
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            height: 2
                            radius: 1
                            color: "#0A84FF"
                            visible: tabDelegate.isTabActive
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 6
                            spacing: 6

                            TinexusVectorIcon {
                                name: "folder"
                                size: 13
                                color: tabDelegate.isTabActive ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.5)
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Text {
                                text: modelData.title
                                color: tabDelegate.isTabActive ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.70)
                                font.pixelSize: 11
                                font.bold: tabDelegate.isTabActive
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                            }

                            // Close Tab Button
                            Rectangle {
                                width: 16
                                height: 16
                                radius: 8
                                color: closeTabMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
                                visible: tabDelegate.isTabActive || tabMouse.containsMouse
                                Layout.alignment: Qt.AlignVCenter
                                z: 2

                                TinexusVectorIcon {
                                    anchors.centerIn: parent
                                    name: "close"
                                    size: 8
                                    color: closeTabMouse.containsMouse ? "#FF453A" : Qt.rgba(1, 1, 1, 0.6)
                                }

                                MouseArea {
                                    id: closeTabMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (typeof bridge !== "undefined") {
                                            bridge.closeTab(modelData.index);
                                        }
                                    }
                                }
                            }
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: function(mouse) {
                                if (typeof bridge === "undefined") return;
                                if (mouse.button === Qt.MiddleButton) {
                                    bridge.closeTab(modelData.index);
                                } else {
                                    bridge.switchTab(modelData.index);
                                }
                            }
                        }
                    }
                }

                // New Tab Button (+)
                Rectangle {
                    width: 26
                    height: 26
                    radius: 6
                    color: newTabMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                    border.color: Qt.rgba(1, 1, 1, 0.10)
                    border.width: 1
                    Layout.alignment: Qt.AlignVCenter

                    TinexusVectorIcon {
                        anchors.centerIn: parent
                        name: "plus"
                        size: 11
                        color: newTabMouse.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.7)
                    }

                    MouseArea {
                        id: newTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") {
                                bridge.createTab();
                            }
                        }
                    }
                }
            }
        }

        // ── 1B. Search Status Banner (Visible when searching) ────────
        Rectangle {
            Layout.fillWidth: true
            height: (typeof bridge !== "undefined" && bridge.isSearching) ? 32 : 0
            visible: height > 0
            clip: true
            color: Qt.rgba(0.04, 0.52, 1.0, 0.12)
            border.color: Qt.rgba(0.04, 0.52, 1.0, 0.30)
            border.width: 1
            z: 19

            Behavior on height {
                NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 10

                TinexusVectorIcon {
                    name: "search"
                    size: 13
                    color: "#0A84FF"
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: typeof bridge !== "undefined"
                          ? ("Searching in \"" + bridge.currentDirName + "\" — " + (bridge.searchResultCount === 0 ? "No items found" : (bridge.searchResultCount + " item" + (bridge.searchResultCount === 1 ? "" : "s") + " found")))
                          : ""
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.bold: true
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                }

                Rectangle {
                    width: 82
                    height: 22
                    radius: 4
                    color: clearBannerMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(1, 1, 1, 0.08)
                    border.color: Qt.rgba(1, 1, 1, 0.15)
                    border.width: 1
                    Layout.alignment: Qt.AlignVCenter

                    Text {
                        anchors.centerIn: parent
                        text: "Clear Search"
                        color: "#FFFFFF"
                        font.pixelSize: 11
                    }

                    MouseArea {
                        id: clearBannerMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            searchInput.text = "";
                            if (typeof bridge !== "undefined") {
                                bridge.clearSearch();
                            }
                        }
                    }
                }
            }
        }

        // ── 2. Main Body: Sidebar + Dynamic Viewports ───────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left Sidebar
            Rectangle {
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                color: "#161922"
                border.color: Qt.rgba(1, 1, 1, 0.06)
                border.width: 1

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true

                    Column {
                        width: 166
                        spacing: 2

                        Repeater {
                            model: typeof bridge !== "undefined" ? bridge.sidebarLocations : []

                            delegate: Item {
                                width: 166
                                height: modelData.isHeader ? 22 : 30

                                // Section Header
                                Text {
                                    visible: modelData.isHeader
                                    anchors.left: parent.left
                                    anchors.leftMargin: 6
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: modelData.label
                                    color: "#86868B"
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                }

                                // Interactive Sidebar Item
                                Rectangle {
                                    visible: !modelData.isHeader
                                    anchors.fill: parent
                                    radius: 6
                                    readonly property bool isCurrent: typeof bridge !== "undefined" && bridge.currentPath === modelData.path
                                    color: isCurrent ? "#0A84FF" : (sbMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.12) : "transparent")

                                    Row {
                                        anchors.fill: parent
                                        anchors.leftMargin: 8
                                        spacing: 8
                                        anchors.verticalCenter: parent.verticalCenter

                                        FileVectorIcon {
                                            iconKind: modelData.iconKind
                                            isDir: true
                                            iconSize: 16
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                        Text {
                                            text: modelData.label
                                            color: "#FFFFFF"
                                            font.pixelSize: 13
                                            font.bold: isCurrent
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    MouseArea {
                                        id: sbMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (typeof bridge !== "undefined" && modelData.path) {
                                                bridge.cd(modelData.path)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Central File Viewport Area
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Background click catcher (handles right-click on empty area)
                // Background click & marquee rubberband selection catcher
                Rectangle {
                    id: rubberBand
                    visible: false
                    color: Qt.rgba(0.04, 0.52, 1.0, 0.20)
                    border.color: "#0A84FF"
                    border.width: 1
                    z: 999
                    property real startX: 0
                    property real startY: 0
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton | Qt.LeftButton

                    onPressed: function(mouse) {
                        if (mouse.button === Qt.LeftButton) {
                            rubberBand.startX = mouse.x;
                            rubberBand.startY = mouse.y;
                            rubberBand.x = mouse.x;
                            rubberBand.y = mouse.y;
                            rubberBand.width = 0;
                            rubberBand.height = 0;
                            rubberBand.visible = true;
                        }
                    }

                    onPositionChanged: function(mouse) {
                        if (rubberBand.visible) {
                            var rx = Math.min(rubberBand.startX, mouse.x);
                            var ry = Math.min(rubberBand.startY, mouse.y);
                            var rw = Math.abs(mouse.x - rubberBand.startX);
                            var rh = Math.abs(mouse.y - rubberBand.startY);
                            rubberBand.x = rx;
                            rubberBand.y = ry;
                            rubberBand.width = rw;
                            rubberBand.height = rh;
                        }
                    }

                    onReleased: function(mouse) {
                        if (rubberBand.visible) {
                            var wasDragged = rubberBand.width > 5 || rubberBand.height > 5;
                            rubberBand.visible = false;
                            if (wasDragged && typeof bridge !== "undefined") {
                                var list = bridge.fileList;
                                var selected = [];
                                if (bridge.currentViewMode === 0) {
                                    var cols = Math.max(1, Math.floor(grid.width / 110));
                                    for (var i = 0; i < list.length; ++i) {
                                        var c = i % cols;
                                        var r = Math.floor(i / cols);
                                        var itemX = c * 110 + 14;
                                        var itemY = r * 115 + 14;
                                        if (!(rubberBand.x > itemX + 100 ||
                                              rubberBand.x + rubberBand.width < itemX ||
                                              rubberBand.y > itemY + 105 ||
                                              rubberBand.y + rubberBand.height < itemY)) {
                                            selected.push(list[i].path);
                                        }
                                    }
                                } else if (bridge.currentViewMode === 1) {
                                    for (var j = 0; j < list.length; ++j) {
                                        var lineY = j * 32 + 32;
                                        if (lineY + 32 >= rubberBand.y && lineY <= rubberBand.y + rubberBand.height) {
                                            selected.push(list[j].path);
                                        }
                                    }
                                }
                                if (selected.length > 0) {
                                    bridge.setSelectedPaths(selected);
                                }
                            }
                        }
                    }

                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton) {
                            rootWindow.contextMenuTarget = typeof bridge !== "undefined" ? bridge.currentPath : "";
                            rootWindow.contextMenuIsDir = true;
                            rootWindow.contextMenuIsBg = true;
                            rootWindow.contextMenuX = mouse.x + 190;
                            rootWindow.contextMenuY = mouse.y + 52;
                            rootWindow.contextMenuVisible = true;
                        } else {
                            rootWindow.contextMenuVisible = false;
                            if (typeof bridge !== "undefined") {
                                bridge.clearSelection();
                            }
                        }
                    }
                }

                // ── VIEW 0: ICONS / GRID VIEW ───────────────────────
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 14
                    clip: true
                    visible: typeof bridge !== "undefined" && bridge.currentViewMode === 0

                    GridView {
                        id: grid
                        width: parent.width
                        cellWidth: 110
                        cellHeight: 115
                        model: typeof bridge !== "undefined" ? bridge.fileList : []

                        delegate: Rectangle {
                            width: 100
                            height: 105
                            radius: 8
                            readonly property bool isSelected: typeof bridge !== "undefined" && bridge.isSelected(modelData.path)
                            color: isSelected ? Qt.rgba(0.04, 0.52, 1.0, 0.35) : (itemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
                            border.color: isSelected ? "#0A84FF" : (itemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent")
                            border.width: 1

                            Column {
                                anchors.centerIn: parent
                                spacing: 6
                                width: 90

                                // Icon + tag dot overlay
                                Item {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 44
                                    height: 44

                                    FileVectorIcon {
                                        anchors.centerIn: parent
                                        iconKind: modelData.iconKind
                                        isDir: modelData.isDir
                                        iconSize: 44
                                    }

                                    // Tag color dot — bottom-right corner
                                    // Uses modelData.tag (embedded in model) for correctness after restart.
                                    // bridge.tagRevision is included so the binding re-evaluates on tag change.
                                    Rectangle {
                                        visible: {
                                            void(bridge.tagRevision) // force re-eval dependency
                                            return modelData.tag !== undefined && modelData.tag !== ""
                                        }
                                        width: 10; height: 10; radius: 5
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        color: {
                                            void(bridge.tagRevision)
                                            var tagColors = {
                                                "Red":    "#FF453A", "Orange": "#FF9F0A",
                                                "Yellow": "#FFD60A", "Green":  "#30D158",
                                                "Blue":   "#0A84FF", "Purple": "#BF5AF2",
                                                "Gray":   "#8E8E93"
                                            }
                                            return tagColors[modelData.tag] || "transparent"
                                        }
                                        border.color: "#181B24"
                                        border.width: 1.5
                                    }
                                }

                                Text {
                                    width: 90
                                    horizontalAlignment: Text.AlignHCenter
                                    text: modelData.name
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                    font.weight: modelData.isDir ? Font.Medium : Font.Normal
                                    elide: Text.ElideMiddle
                                    maximumLineCount: 2
                                    wrapMode: Text.Wrap
                                }

                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: modelData.sizeStr
                                    color: Qt.rgba(1, 1, 1, 0.45)
                                    font.pixelSize: 10
                                }
                            }

                            MouseArea {
                                id: itemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                cursorShape: Qt.PointingHandCursor

                                onClicked: function(mouse) {
                                    if (typeof bridge !== "undefined") {
                                        if (mouse.modifiers & Qt.ControlModifier) {
                                            bridge.toggleSelectItem(modelData.path);
                                        } else if (mouse.modifiers & Qt.ShiftModifier) {
                                            bridge.selectRange(modelData.path);
                                        } else {
                                            if (mouse.button === Qt.RightButton && bridge.isSelected(modelData.path)) {
                                                // keep multi-selection intact for right-click context menu
                                            } else {
                                                bridge.selectItem(modelData.path);
                                            }
                                        }
                                    }
                                    if (mouse.button === Qt.RightButton) {
                                        rootWindow.contextMenuTarget = modelData.path;
                                        rootWindow.contextMenuIsDir = modelData.isDir;
                                        rootWindow.contextMenuIsBg = false;
                                        rootWindow.contextMenuX = mouse.x + itemMouse.mapToItem(rootWindow.contentItem, 0, 0).x;
                                        rootWindow.contextMenuY = mouse.y + itemMouse.mapToItem(rootWindow.contentItem, 0, 0).y;
                                        rootWindow.contextMenuVisible = true;
                                    } else {
                                        rootWindow.contextMenuVisible = false;
                                    }
                                }

                                onDoubleClicked: function(mouse) {
                                    if (mouse.button === Qt.LeftButton && typeof bridge !== "undefined") {
                                        bridge.openItem(modelData.path, modelData.isDir);
                                    }
                                }
                            }
                        }
                    }
                }

                // ── VIEW 1: LIST / TABLE VIEW ───────────────────────
                Item {
                    anchors.fill: parent
                    visible: typeof bridge !== "undefined" && bridge.currentViewMode === 1

                    // Table Column Headers — Phase 1: clickable sort
                    Rectangle {
                        id: listHeader
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 28
                        color: "#1E222E"
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1

                        // Helper: tag name list must match TagManager::S_TAGS order
                        readonly property var tagNames: ["Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray"]

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 10

                            // ── Name column header
                            Item {
                                Layout.preferredWidth: 320
                                height: parent.height
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    Text {
                                        text: "Name"
                                        color: (typeof bridge !== "undefined" && bridge.sortColumn === "name") ? "#FFFFFF" : "#86868B"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    TinexusVectorIcon {
                                        visible: typeof bridge !== "undefined" && bridge.sortColumn === "name"
                                        name: (typeof bridge !== "undefined" && bridge.sortAscending) ? "sort-asc" : "sort-desc"
                                        size: 8
                                        color: "#0A84FF"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: {
                                        if (typeof bridge !== "undefined")
                                            bridge.sortBy("name", bridge.sortColumn !== "name" ? true : !bridge.sortAscending)
                                    }
                                }
                            }

                            // ── Date Modified column header
                            Item {
                                Layout.preferredWidth: 160
                                height: parent.height
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    Text {
                                        text: "Date Modified"
                                        color: (typeof bridge !== "undefined" && bridge.sortColumn === "modified") ? "#FFFFFF" : "#86868B"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    TinexusVectorIcon {
                                        visible: typeof bridge !== "undefined" && bridge.sortColumn === "modified"
                                        name: (typeof bridge !== "undefined" && bridge.sortAscending) ? "sort-asc" : "sort-desc"
                                        size: 8
                                        color: "#0A84FF"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: {
                                        if (typeof bridge !== "undefined")
                                            bridge.sortBy("modified", bridge.sortColumn !== "modified" ? true : !bridge.sortAscending)
                                    }
                                }
                            }

                            // ── Size column header
                            Item {
                                Layout.preferredWidth: 90
                                height: parent.height
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    Text {
                                        text: "Size"
                                        color: (typeof bridge !== "undefined" && bridge.sortColumn === "size") ? "#FFFFFF" : "#86868B"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    TinexusVectorIcon {
                                        visible: typeof bridge !== "undefined" && bridge.sortColumn === "size"
                                        name: (typeof bridge !== "undefined" && bridge.sortAscending) ? "sort-asc" : "sort-desc"
                                        size: 8
                                        color: "#0A84FF"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: {
                                        if (typeof bridge !== "undefined")
                                            bridge.sortBy("size", bridge.sortColumn !== "size" ? true : !bridge.sortAscending)
                                    }
                                }
                            }

                            // ── Kind column header
                            Item {
                                Layout.fillWidth: true
                                height: parent.height
                                Row {
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 4
                                    Text {
                                        text: "Kind"
                                        color: (typeof bridge !== "undefined" && bridge.sortColumn === "kind") ? "#FFFFFF" : "#86868B"
                                        font.pixelSize: 11
                                        font.bold: true
                                    }
                                    TinexusVectorIcon {
                                        visible: typeof bridge !== "undefined" && bridge.sortColumn === "kind"
                                        name: (typeof bridge !== "undefined" && bridge.sortAscending) ? "sort-asc" : "sort-desc"
                                        size: 8
                                        color: "#0A84FF"
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    hoverEnabled: true
                                    onClicked: {
                                        if (typeof bridge !== "undefined")
                                            bridge.sortBy("kind", bridge.sortColumn !== "kind" ? true : !bridge.sortAscending)
                                    }
                                }
                            }
                        }
                    }

                    ScrollView {
                        anchors.top: listHeader.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        clip: true

                        ListView {
                            width: parent.width
                            model: typeof bridge !== "undefined" ? bridge.fileList : []

                            delegate: Rectangle {
                                width: parent.width
                                height: 32
                                readonly property bool isSelected: typeof bridge !== "undefined" && bridge.isSelected(modelData.path)
                                color: isSelected ? Qt.rgba(0.04, 0.52, 1.0, 0.35) : (rowMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : (index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.02)))

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 14
                                    spacing: 10

                                    Row {
                                        Layout.preferredWidth: 320
                                        spacing: 8
                                        anchors.verticalCenter: parent.verticalCenter

                                        FileVectorIcon {
                                            iconKind: modelData.iconKind
                                            isDir: modelData.isDir
                                            iconSize: 20
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            text: (typeof bridge !== "undefined" && bridge.isSearching && modelData.relativePath) ? modelData.relativePath : modelData.name
                                            color: "#FFFFFF"
                                            font.pixelSize: 12
                                            elide: Text.ElideRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }

                                    Text {
                                        text: modelData.modified
                                        color: Qt.rgba(1, 1, 1, 0.6)
                                        font.pixelSize: 11
                                        Layout.preferredWidth: 160
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Text {
                                        text: modelData.sizeStr
                                        color: Qt.rgba(1, 1, 1, 0.6)
                                        font.pixelSize: 11
                                        Layout.preferredWidth: 90
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Text {
                                        text: modelData.isDir ? "Folder" : modelData.iconKind.toUpperCase()
                                        color: Qt.rgba(1, 1, 1, 0.5)
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }

                                MouseArea {
                                    id: rowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    cursorShape: Qt.PointingHandCursor

                                    onClicked: function(mouse) {
                                        if (typeof bridge !== "undefined") {
                                            if (mouse.modifiers & Qt.ControlModifier) {
                                                bridge.toggleSelectItem(modelData.path);
                                            } else if (mouse.modifiers & Qt.ShiftModifier) {
                                                bridge.selectRange(modelData.path);
                                            } else {
                                                if (mouse.button === Qt.RightButton && bridge.isSelected(modelData.path)) {
                                                    // keep multi-selection intact for right-click context menu
                                                } else {
                                                    bridge.selectItem(modelData.path);
                                                }
                                            }
                                        }
                                        if (mouse.button === Qt.RightButton) {
                                            rootWindow.contextMenuTarget = modelData.path;
                                            rootWindow.contextMenuIsDir = modelData.isDir;
                                            rootWindow.contextMenuIsBg = false;
                                            rootWindow.contextMenuX = mouse.x + rowMouse.mapToItem(rootWindow.contentItem, 0, 0).x;
                                            rootWindow.contextMenuY = mouse.y + rowMouse.mapToItem(rootWindow.contentItem, 0, 0).y;
                                            rootWindow.contextMenuVisible = true;
                                        } else {
                                            rootWindow.contextMenuVisible = false;
                                        }
                                    }

                                    onDoubleClicked: function(mouse) {
                                        if (mouse.button === Qt.LeftButton && typeof bridge !== "undefined") {
                                            bridge.openItem(modelData.path, modelData.isDir);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ── VIEW 2: MULTI-COLUMN BROWSER WITH RIGHT-SIDE INSPECTOR PANE ──
                RowLayout {
                    anchors.fill: parent
                    spacing: 0
                    visible: typeof bridge !== "undefined" && bridge.currentViewMode === 2

                    // Horizontal Column Browser
                    ScrollView {
                        id: colScrollView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentHeight: availableHeight

                        ScrollBar.horizontal: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        Connections {
                            target: typeof bridge !== "undefined" ? bridge : null
                            function onColumnsDataChanged() {
                                Qt.callLater(function() {
                                    if (colRow.width > colScrollView.width) {
                                        var targetX = colRow.width - colScrollView.width;
                                        colScrollAnim.to = targetX;
                                        colScrollAnim.restart();
                                    }
                                });
                            }
                        }

                        NumberAnimation {
                            id: colScrollAnim
                            target: colScrollView.contentItem
                            property: "contentX"
                            duration: 220
                            easing.type: Easing.OutCubic
                        }

                        Row {
                            id: colRow
                            height: parent.height
                            spacing: 0

                            Repeater {
                                model: typeof bridge !== "undefined" ? bridge.columnsData : []

                                delegate: Rectangle {
                                    id: colRect
                                    readonly property var colData: modelData
                                    readonly property int colIdx: colData.columnIndex
                                    width: 220
                                    height: parent.height
                                    color: "#181B24"
                                    border.color: Qt.rgba(1, 1, 1, 0.08)
                                    border.width: 1

                                    ListView {
                                        id: colList
                                        anchors.fill: parent
                                        anchors.margins: 4
                                        clip: true
                                        model: colData.items || []
                                        boundsBehavior: Flickable.StopAtBounds

                                        ScrollBar.vertical: ScrollBar {
                                            policy: colList.contentHeight > colList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                                        }

                                        delegate: Rectangle {
                                            id: colItemRect
                                            width: colList.width
                                            height: 30
                                            radius: 5
                                            readonly property bool isSelected: (typeof bridge !== "undefined" && bridge.getColumnSelectedIndex(colIdx) === index)
                                            color: isSelected ? "#0A84FF" : (cMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent")

                                            DropArea {
                                                anchors.fill: parent
                                                enabled: modelData.isDir
                                                onDropped: function(drop) {
                                                    var src = drop.getDataAsString("text/plain");
                                                    if (src && src !== modelData.path && typeof bridge !== "undefined") {
                                                        bridge.moveItem(src, modelData.path);
                                                    }
                                                }
                                            }

                                            Item {
                                                id: dragItem
                                                Drag.active: cMouse.drag.active
                                                Drag.source: modelData.path
                                                Drag.mimeData: { "text/plain": modelData.path }
                                            }

                                            Row {
                                                anchors.fill: parent
                                                anchors.leftMargin: 8
                                                anchors.rightMargin: 8
                                                spacing: 6
                                                anchors.verticalCenter: parent.verticalCenter

                                                FileVectorIcon {
                                                    iconKind: modelData.iconKind
                                                    isDir: modelData.isDir
                                                    iconSize: 16
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }

                                                Text {
                                                    text: modelData.name
                                                    color: "#FFFFFF"
                                                    font.pixelSize: 12
                                                    font.bold: isSelected
                                                    elide: Text.ElideRight
                                                    width: parent.width - 44
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }

                                                TinexusVectorIcon {
                                                    visible: modelData.isDir
                                                    name: "chevron-right-small"
                                                    size: 9
                                                    color: isSelected ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.35)
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }

                                            MouseArea {
                                                id: cMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                cursorShape: Qt.PointingHandCursor
                                                drag.target: dragItem
                                                drag.axis: Drag.XAndYAxis

                                                onReleased: function() {
                                                    if (dragItem.Drag.active) {
                                                        dragItem.Drag.drop();
                                                    }
                                                }

                                                onClicked: function(mouse) {
                                                    if (typeof bridge !== "undefined") {
                                                        bridge.selectColumnItem(colIdx, index);
                                                    }
                                                    if (mouse.button === Qt.RightButton) {
                                                        rootWindow.contextMenuTarget = modelData.path;
                                                        rootWindow.contextMenuIsDir = modelData.isDir;
                                                        rootWindow.contextMenuIsBg = false;
                                                        rootWindow.contextMenuX = mouse.x + cMouse.mapToItem(rootWindow.contentItem, 0, 0).x;
                                                        rootWindow.contextMenuY = mouse.y + cMouse.mapToItem(rootWindow.contentItem, 0, 0).y;
                                                        rootWindow.contextMenuVisible = true;
                                                    } else {
                                                        rootWindow.contextMenuVisible = false;
                                                    }
                                                }

                                                onDoubleClicked: function(mouse) {
                                                    if (mouse.button === Qt.LeftButton && typeof bridge !== "undefined") {
                                                        bridge.openColumnItem(colIdx, index);
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // ── RIGHT-SIDE INSPECTOR / DETAILS PANE ──────────
                    Rectangle {
                        Layout.preferredWidth: 260
                        Layout.fillHeight: true
                        color: "#151821"
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1

                        readonly property var details: typeof bridge !== "undefined" ? bridge.selectedItemDetails : null

                        Column {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 12

                            // Large File/Folder Preview Box
                            Rectangle {
                                width: parent.width
                                height: 130
                                radius: 8
                                color: Qt.rgba(0, 0, 0, 0.35)
                                border.color: Qt.rgba(1, 1, 1, 0.10)
                                border.width: 1

                                Image {
                                    anchors.centerIn: parent
                                    width: Math.min(parent.width - 16, 180)
                                    height: Math.min(parent.height - 16, 110)
                                    fillMode: Image.PreserveAspectFit
                                    source: (details && details.iconKind === "image") ? ("file://" + details.path) : ""
                                    visible: details && details.iconKind === "image"
                                    asynchronous: true
                                    cache: true
                                }

                                FileVectorIcon {
                                    anchors.centerIn: parent
                                    iconKind: details ? details.iconKind : "file"
                                    isDir: details ? details.isDir : false
                                    iconSize: 64
                                    visible: !details || details.iconKind !== "image"
                                }
                            }

                            // File Name
                            Text {
                                text: details ? details.name : "No Selection"
                                color: "#FFFFFF"
                                font.pixelSize: 14
                                font.bold: true
                                elide: Text.ElideMiddle
                                width: parent.width
                                horizontalAlignment: Text.AlignHCenter
                            }

                            Rectangle {
                                width: parent.width
                                height: 1
                                color: Qt.rgba(1, 1, 1, 0.08)
                            }

                            // Metadata Fields
                            Grid {
                                columns: 2
                                width: parent.width
                                rowSpacing: 6
                                columnSpacing: 8

                                Text { text: "Kind:"; color: "#86868B"; font.pixelSize: 11; font.weight: Font.Medium }
                                Text { text: details ? details.kind : "--"; color: "#FFFFFF"; font.pixelSize: 11; elide: Text.ElideRight; width: 155 }

                                Text { text: "Size:"; color: "#86868B"; font.pixelSize: 11; font.weight: Font.Medium }
                                Text { text: details ? details.sizeStr : "--"; color: "#FFFFFF"; font.pixelSize: 11 }

                                Text { text: "Created:"; color: "#86868B"; font.pixelSize: 11; font.weight: Font.Medium }
                                Text { text: (details && details.created) ? details.created : "--"; color: "#FFFFFF"; font.pixelSize: 11; elide: Text.ElideRight; width: 155 }

                                Text { text: "Modified:"; color: "#86868B"; font.pixelSize: 11; font.weight: Font.Medium }
                                Text { text: details ? details.modified : "--"; color: "#FFFFFF"; font.pixelSize: 11; elide: Text.ElideRight; width: 155 }

                                Text { text: "Permissions:"; color: "#86868B"; font.pixelSize: 11; font.weight: Font.Medium }
                                Text { text: (details && details.permissions) ? details.permissions : "--"; color: "#FFFFFF"; font.pixelSize: 11; font.family: "Monospace" }

                                Text { text: "Owner:"; color: "#86868B"; font.pixelSize: 11; font.weight: Font.Medium }
                                Text { text: (details && details.owner) ? (details.owner + (details.group ? " (" + details.group + ")" : "")) : "--"; color: "#FFFFFF"; font.pixelSize: 11 }

                                Text { text: "Where:"; color: "#86868B"; font.pixelSize: 11; font.weight: Font.Medium }
                                Text { text: (details && details.fullPath) ? details.fullPath : "--"; color: "#98989D"; font.pixelSize: 10; elide: Text.ElideMiddle; width: 155 }
                            }

                            Item { width: parent.width; height: 10 }

                            // Action Buttons: Open & Trash
                            Rectangle {
                                width: parent.width
                                height: 32
                                radius: 6
                                color: "#0A84FF"
                                visible: details && details.name !== "No Selection"

                                Text {
                                    anchors.centerIn: parent
                                    text: "Open"
                                    color: "#FFFFFF"
                                    font.bold: true
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (details && typeof bridge !== "undefined") {
                                            bridge.openItem(details.path, details.isDir);
                                        }
                                    }
                                }
                            }

                            Rectangle {
                                width: parent.width
                                height: 28
                                radius: 6
                                color: Qt.rgba(1, 1, 1, 0.08)
                                border.color: Qt.rgba(1, 1, 1, 0.12)
                                border.width: 1
                                visible: details && details.name !== "No Selection"

                                Text {
                                    anchors.centerIn: parent
                                    text: "Move to Trash"
                                    color: "#FF453A"
                                    font.pixelSize: 11
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (details && typeof bridge !== "undefined") {
                                            bridge.deleteItem(details.path);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ── VIEW 3: GALLERY VIEW ────────────────────────────
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10
                    visible: typeof bridge !== "undefined" && bridge.currentViewMode === 3

                    // Large Gallery Preview Hero
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 16
                        radius: 12
                        color: Qt.rgba(0, 0, 0, 0.40)
                        border.color: Qt.rgba(1, 1, 1, 0.10)
                        border.width: 1

                        readonly property var details: typeof bridge !== "undefined" ? bridge.selectedItemDetails : null

                        Column {
                            anchors.centerIn: parent
                            spacing: 12

                            FileVectorIcon {
                                anchors.horizontalCenter: parent.horizontalCenter
                                iconKind: parent.parent.details ? parent.parent.details.iconKind : "file"
                                isDir: parent.parent.details ? parent.parent.details.isDir : false
                                iconSize: 110
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: parent.parent.details ? parent.parent.details.name : "Select an item"
                                color: "#FFFFFF"
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: parent.parent.details ? (parent.parent.details.kind + " • " + parent.parent.details.sizeStr) : ""
                                color: Qt.rgba(1, 1, 1, 0.6)
                                font.pixelSize: 12
                            }
                        }
                    }

                    // Bottom Filmstrip Carousel
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 90
                        color: "#151821"
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 8
                            clip: true

                            ListView {
                                orientation: ListView.Horizontal
                                spacing: 10
                                model: typeof bridge !== "undefined" ? bridge.fileList : []

                                delegate: Rectangle {
                                    width: 70
                                    height: 70
                                    radius: 6
                                    readonly property bool isSelected: typeof bridge !== "undefined" && bridge.isSelected(modelData.path)
                                    color: isSelected ? Qt.rgba(0.04, 0.52, 1.0, 0.4) : (gMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.10) : "transparent")
                                    border.color: isSelected ? "#0A84FF" : "transparent"
                                    border.width: 1.5

                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 4

                                        FileVectorIcon {
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            iconKind: modelData.iconKind
                                            isDir: modelData.isDir
                                            iconSize: 32
                                        }

                                        Text {
                                            text: modelData.name
                                            color: "#FFFFFF"
                                            font.pixelSize: 10
                                            elide: Text.ElideMiddle
                                            width: 60
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }

                                    MouseArea {
                                        id: gMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: function(mouse) {
                                            if (typeof bridge !== "undefined") {
                                                if (mouse.modifiers & Qt.ControlModifier) {
                                                    bridge.toggleSelectItem(modelData.path);
                                                } else if (mouse.modifiers & Qt.ShiftModifier) {
                                                    bridge.selectRange(modelData.path);
                                                } else {
                                                    bridge.selectItem(modelData.path);
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ── 3. Right-Click Context Menu Modal ───────────────────────────
    Rectangle {
        id: contextMenu
        visible: rootWindow.contextMenuVisible
        x: Math.min(rootWindow.width - width - 8, rootWindow.contextMenuX)
        y: Math.min(rootWindow.height - height - 8, rootWindow.contextMenuY)
        width: 195
        height: rootWindow.contextMenuIsBg ? 150 : 280
        radius: 8
        color: Qt.rgba(24 / 255.0, 27 / 255.0, 36 / 255.0, 0.96)
        border.color: Qt.rgba(1, 1, 1, 0.16)
        border.width: 1
        z: 99

        Column {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 2

            // Background Actions
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: rootWindow.contextMenuIsBg
                color: bgNewFMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: "New Folder"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: bgNewFMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { rootWindow.contextMenuVisible = false; rootWindow.newFolderDialogVisible = true; }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: rootWindow.contextMenuIsBg
                color: bgNewDocMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: "New Document"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: bgNewDocMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        rootWindow.contextMenuVisible = false;
                        if (typeof bridge !== "undefined") bridge.createNewFile("Untitled.txt");
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: rootWindow.contextMenuIsBg
                color: bgPasteMouse.containsMouse ? "#0A84FF" : "transparent"
                opacity: (typeof bridge !== "undefined" && bridge.hasClipboard) ? 1.0 : 0.4
                Text { text: "Paste"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: bgPasteMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    enabled: typeof bridge !== "undefined" && bridge.hasClipboard
                    onClicked: { rootWindow.contextMenuVisible = false; if (typeof bridge !== "undefined") bridge.pasteItem(); }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: rootWindow.contextMenuIsBg
                color: bgRefMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: "Refresh"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: bgRefMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { rootWindow.contextMenuVisible = false; if (typeof bridge !== "undefined") bridge.refresh(); }
                }
            }

            // Item Specific Actions
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg
                color: itmOpenMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: "Open"; color: "#FFFFFF"; font.pixelSize: 12; font.bold: true; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmOpenMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { rootWindow.contextMenuVisible = false; if (typeof bridge !== "undefined") bridge.openItem(rootWindow.contextMenuTarget, rootWindow.contextMenuIsDir); }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg
                color: itmQuickLookMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: "Quick Look (Space)"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmQuickLookMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        rootWindow.contextMenuVisible = false;
                        if (typeof bridge !== "undefined") {
                            bridge.selectItem(rootWindow.contextMenuTarget);
                            if (bridge.currentViewMode !== 2) bridge.setViewMode(2);
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg
                color: itmGetInfoMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: "Get Info"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmGetInfoMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        rootWindow.contextMenuVisible = false;
                        if (typeof bridge !== "undefined") {
                            bridge.selectItem(rootWindow.contextMenuTarget);
                            if (bridge.currentViewMode !== 2) bridge.setViewMode(2);
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg
                color: itmCopyMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: typeof bridge !== "undefined" && bridge.selectedCount > 1 ? ("Copy (" + bridge.selectedCount + " items)") : "Copy"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmCopyMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        rootWindow.contextMenuVisible = false;
                        if (typeof bridge !== "undefined") {
                            if (bridge.selectedCount > 1) {
                                bridge.copySelected();
                            } else {
                                bridge.copyItem(rootWindow.contextMenuTarget);
                            }
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg
                color: itmCutMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: typeof bridge !== "undefined" && bridge.selectedCount > 1 ? ("Cut (" + bridge.selectedCount + " items)") : "Cut"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmCutMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        rootWindow.contextMenuVisible = false;
                        if (typeof bridge !== "undefined") {
                            if (bridge.selectedCount > 1) {
                                bridge.cutSelected();
                            } else {
                                bridge.cutItem(rootWindow.contextMenuTarget);
                            }
                        }
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg && (typeof bridge === "undefined" || bridge.selectedCount <= 1)
                color: itmRenMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: "Rename..."; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmRenMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        rootWindow.contextMenuVisible = false;
                        rootWindow.renameTarget = rootWindow.contextMenuTarget;
                        rootWindow.renameDialogVisible = true;
                    }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg
                color: itmDelMouse.containsMouse ? "#FF453A" : "transparent"
                Text { text: typeof bridge !== "undefined" && bridge.selectedCount > 1 ? ("Move " + bridge.selectedCount + " items to Trash") : "Move to Trash"; color: itmDelMouse.containsMouse ? "#FFFFFF" : "#FF453A"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmDelMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        rootWindow.contextMenuVisible = false;
                        if (typeof bridge !== "undefined") {
                            if (bridge.selectedCount > 1) {
                                bridge.deleteSelected();
                            } else {
                                bridge.deleteItem(rootWindow.contextMenuTarget);
                            }
                        }
                    }
                }
            }

            // Color Tags Divider & Row — Phase 1: actually calls setTagOnItem
            Rectangle {
                width: parent.width - 12; height: 1
                anchors.horizontalCenter: parent.horizontalCenter
                color: Qt.rgba(1, 1, 1, 0.1)
                visible: !rootWindow.contextMenuIsBg
            }

            // Tag names must match TagManager::S_TAGS order exactly
            property var _tagNames: ["Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray"]

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6
                visible: !rootWindow.contextMenuIsBg
                height: 28

                Repeater {
                    model: ["#FF453A", "#FF9F0A", "#FFD60A", "#30D158", "#0A84FF", "#BF5AF2", "#8E8E93"]
                    delegate: Rectangle {
                        width: 16; height: 16; radius: 8
                        color: modelData
                        anchors.verticalCenter: parent.verticalCenter

                        // Check if this tag is currently applied to target
                        // Uses bridge.getTagForItem() here because contextMenuTarget is not a model item.
                        // bridge.tagRevision in the binding forces re-eval after tag change.
                        readonly property bool isApplied: {
                            if (typeof bridge === "undefined" || rootWindow.contextMenuTarget === "") return false
                            void(bridge.tagRevision) // reactivity dependency
                            var tagNames = ["Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray"]
                            return bridge.getTagForItem(rootWindow.contextMenuTarget) === tagNames[index]
                        }

                        border.color: isApplied ? "#FFFFFF" : (tagMouse.containsMouse ? Qt.rgba(1,1,1,0.8) : "transparent")
                        border.width: isApplied ? 2 : 1.5

                        TinexusVectorIcon {
                            visible: isApplied
                            anchors.centerIn: parent
                            name: "check"
                            size: 9
                            color: "#FFFFFF"
                        }

                        MouseArea {
                            id: tagMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    var tagNames = ["Red", "Orange", "Yellow", "Green", "Blue", "Purple", "Gray"]
                                    var tagName = tagNames[index]
                                    if (bridge.selectedCount > 1) {
                                        bridge.setTagOnSelected(tagName);
                                    } else if (rootWindow.contextMenuTarget !== "") {
                                        var current = bridge.getTagForItem(rootWindow.contextMenuTarget)
                                        if (current === tagName) {
                                            bridge.setTagOnItem(rootWindow.contextMenuTarget, "")
                                        } else {
                                            bridge.setTagOnItem(rootWindow.contextMenuTarget, tagName)
                                        }
                                    }
                                }
                                rootWindow.contextMenuVisible = false
                            }
                        }
                    }
                }
            }
        }
    }

    // ── 4. New Folder Dialog Modal ──────────────────────────────────
    Rectangle {
        visible: rootWindow.newFolderDialogVisible
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.6)
        z: 100

        Rectangle {
            anchors.centerIn: parent
            width: 320
            height: 140
            radius: 12
            color: "#1E222E"
            border.color: Qt.rgba(1, 1, 1, 0.16)
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text { text: "New Folder Name:"; color: "#FFFFFF"; font.pixelSize: 13; font.bold: true }

                Rectangle {
                    width: parent.width
                    height: 32
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.4)
                    border.color: "#0A84FF"
                    border.width: 1

                    TextInput {
                        id: newFolderInput
                        anchors.fill: parent
                        anchors.margins: 6
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        focus: rootWindow.newFolderDialogVisible
                        text: "Untitled Folder"
                        selectByMouse: true
                    }
                }

                Row {
                    spacing: 10
                    anchors.right: parent.right

                    Rectangle {
                        width: 70; height: 28; radius: 6; color: Qt.rgba(1, 1, 1, 0.1)
                        Text { anchors.centerIn: parent; text: "Cancel"; color: "#FFFFFF"; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: rootWindow.newFolderDialogVisible = false }
                    }

                    Rectangle {
                        width: 70; height: 28; radius: 6; color: "#0A84FF"
                        Text { anchors.centerIn: parent; text: "Create"; color: "#FFFFFF"; font.bold: true; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") bridge.createNewFolder(newFolderInput.text);
                                rootWindow.newFolderDialogVisible = false;
                            }
                        }
                    }
                }
            }
        }
    }

    // ── 5. Rename Dialog Modal ──────────────────────────────────────
    Rectangle {
        visible: rootWindow.renameDialogVisible
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.6)
        z: 100

        Rectangle {
            anchors.centerIn: parent
            width: 320
            height: 140
            radius: 12
            color: "#1E222E"
            border.color: Qt.rgba(1, 1, 1, 0.16)
            border.width: 1

            Column {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text { text: "Rename Item:"; color: "#FFFFFF"; font.pixelSize: 13; font.bold: true }

                Rectangle {
                    width: parent.width
                    height: 32
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.4)
                    border.color: "#0A84FF"
                    border.width: 1

                    TextInput {
                        id: renameInput
                        anchors.fill: parent
                        anchors.margins: 6
                        color: "#FFFFFF"
                        font.pixelSize: 13
                        focus: rootWindow.renameDialogVisible
                        text: rootWindow.renameTarget.split("/").pop()
                        selectByMouse: true
                    }
                }

                Row {
                    spacing: 10
                    anchors.right: parent.right

                    Rectangle {
                        width: 70; height: 28; radius: 6; color: Qt.rgba(1, 1, 1, 0.1)
                        Text { anchors.centerIn: parent; text: "Cancel"; color: "#FFFFFF"; font.pixelSize: 12 }
                        MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: rootWindow.renameDialogVisible = false }
                    }

                    Rectangle {
                        width: 70; height: 28; radius: 6; color: "#0A84FF"
                        Text { anchors.centerIn: parent; text: "Rename"; color: "#FFFFFF"; font.bold: true; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") bridge.renameItem(rootWindow.renameTarget, renameInput.text);
                                rootWindow.renameDialogVisible = false;
                            }
                        }
                    }
                }
            }
        }
    }

    // ── Phase 2A: Async File Operation Progress Toast / Card ────────────────
    Rectangle {
        id: operationCard
        visible: typeof bridge !== "undefined" && bridge.isOperating
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: 320
        height: 74
        radius: 10
        color: "#181B24"
        border.color: "#2C3240"
        border.width: 1
        z: 9999

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            Row {
                width: parent.width
                spacing: 8

                Text {
                    id: opTitle
                    text: typeof bridge !== "undefined" ? bridge.currentOperationName : "Working..."
                    color: "#FFFFFF"
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                    width: parent.width - cancelBtn.width - 8
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    id: cancelBtn
                    width: 54
                    height: 20
                    radius: 4
                    color: cancelMouse.containsMouse ? "#FF453A" : Qt.rgba(1, 1, 1, 0.1)
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "Cancel"
                        color: "#FFFFFF"
                        font.pixelSize: 11
                    }

                    MouseArea {
                        id: cancelMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (typeof bridge !== "undefined") {
                                bridge.cancelCurrentOperation()
                            }
                        }
                    }
                }
            }

            // Progress bar track
            Rectangle {
                width: parent.width
                height: 6
                radius: 3
                color: Qt.rgba(1, 1, 1, 0.1)

                Rectangle {
                    height: parent.height
                    radius: 3
                    color: "#0A84FF"
                    width: parent.width * (typeof bridge !== "undefined" ? Math.min(Math.max(bridge.operationProgress, 0.0), 1.0) : 0.0)

                    Behavior on width {
                        NumberAnimation { duration: 100 }
                    }
                }
            }

            // Telemetry label: % and speed
            Row {
                width: parent.width

                Text {
                    text: {
                        if (typeof bridge === "undefined") return "0%"
                        var pct = Math.round(bridge.operationProgress * 100)
                        return pct + "%"
                    }
                    color: Qt.rgba(1, 1, 1, 0.6)
                    font.pixelSize: 10
                }

                Item { Layout.fillWidth: true; width: 10 }

                Text {
                    anchors.right: parent.right
                    text: typeof bridge !== "undefined" && bridge.operationSpeedStr !== "" ? bridge.operationSpeedStr : ""
                    color: "#30D158"
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }
    }

    // =========================================================================
    // PHASE 2B: INTERACTIVE CONFLICT RESOLUTION MODAL DIALOG
    // =========================================================================
    Rectangle {
        id: conflictModalOverlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.65)
        visible: typeof bridge !== "undefined" && bridge.conflictDialogVisible
        z: 9999

        // Consume mouse clicks so underlying window cannot be clicked
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            hoverEnabled: true
            preventStealing: true
        }

        // Modal Dialog Card
        Rectangle {
            id: conflictCard
            width: 480
            height: cardContent.height + 40
            radius: 12
            color: "#1E222D"
            border.color: "#2E3445"
            border.width: 1
            anchors.centerIn: parent

            Column {
                id: cardContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 20
                spacing: 16

                // Header with Warning / Duplicate Icon
                Row {
                    spacing: 12
                    width: parent.width

                    Rectangle {
                        width: 36
                        height: 36
                        radius: 8
                        color: (typeof bridge !== "undefined" && bridge.conflictDetails && bridge.conflictDetails.isTypeMismatch) ? Qt.rgba(1, 0.27, 0.23, 0.15) : Qt.rgba(1, 0.62, 0.04, 0.15)

                        TinexusVectorIcon {
                            anchors.centerIn: parent
                            name: (typeof bridge !== "undefined" && bridge.conflictDetails && bridge.conflictDetails.isTypeMismatch) ? "warning" : "document"
                            size: 18
                            color: (typeof bridge !== "undefined" && bridge.conflictDetails && bridge.conflictDetails.isTypeMismatch) ? "#FF453A" : "#FF9F0A"
                        }
                    }

                    Column {
                        width: parent.width - 48
                        spacing: 4

                        Text {
                            text: {
                                var name = (typeof bridge !== "undefined" && bridge.conflictDetails) ? (bridge.conflictDetails.fileName || "") : "";
                                return "An item named \"" + name + "\" already exists."
                            }
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.bold: true
                            elide: Text.ElideMiddle
                            width: parent.width
                        }

                        Text {
                            text: "Do you want to replace it with the new item you are pasting?"
                            color: Qt.rgba(1, 1, 1, 0.6)
                            font.pixelSize: 12
                            width: parent.width
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // Type mismatch warning banner (Safeguard)
                Rectangle {
                    width: parent.width
                    height: typeMismatchCol.height + 16
                    radius: 8
                    color: Qt.rgba(1, 0.27, 0.23, 0.12)
                    border.color: "#FF453A"
                    visible: typeof bridge !== "undefined" && bridge.conflictDetails ? Boolean(bridge.conflictDetails.isTypeMismatch) : false

                    Column {
                        id: typeMismatchCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 8
                        spacing: 4

                        Text {
                            text: "WARNING: Type Mismatch (Folder vs File)"
                            color: "#FF453A"
                            font.pixelSize: 11
                            font.bold: true
                        }

                        Text {
                            text: {
                                if (typeof bridge === "undefined" || !bridge.conflictDetails) return "";
                                var count = bridge.conflictDetails.destDirItemCount || 0;
                                var sizeStr = bridge.conflictDetails.destDirTotalSizeStr || "0 B";
                                return "Replacing this folder will permanently delete all " + count + " items (" + sizeStr + ") inside it."
                            }
                            color: Qt.rgba(1, 1, 1, 0.85)
                            font.pixelSize: 11
                            width: parent.width
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // Comparison Cards (Existing vs Incoming)
                Row {
                    width: parent.width
                    spacing: 12

                    // Existing Item Card
                    Rectangle {
                        width: (parent.width - 12) / 2
                        height: 72
                        radius: 8
                        color: Qt.rgba(1, 1, 1, 0.04)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            Text {
                                text: "Existing Item"
                                color: Qt.rgba(1, 1, 1, 0.5)
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Text {
                                text: typeof bridge !== "undefined" && bridge.conflictDetails ? (bridge.conflictDetails.destSizeStr || "0 B") : ""
                                color: "#FFFFFF"
                                font.pixelSize: 12
                                font.bold: true
                            }

                            Text {
                                text: typeof bridge !== "undefined" && bridge.conflictDetails ? (bridge.conflictDetails.destMtimeStr || "") : ""
                                color: Qt.rgba(1, 1, 1, 0.45)
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                width: parent.width
                            }
                        }
                    }

                    // Incoming Item Card
                    Rectangle {
                        width: (parent.width - 12) / 2
                        height: 72
                        radius: 8
                        color: Qt.rgba(10, 132, 255, 0.08)
                        border.color: Qt.rgba(10, 132, 255, 0.25)
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            Text {
                                text: "Incoming Item"
                                color: "#0A84FF"
                                font.pixelSize: 10
                                font.bold: true
                            }

                            Text {
                                text: typeof bridge !== "undefined" && bridge.conflictDetails ? (bridge.conflictDetails.sourceSizeStr || "0 B") : ""
                                color: "#FFFFFF"
                                font.pixelSize: 12
                                font.bold: true
                            }

                            Text {
                                text: typeof bridge !== "undefined" && bridge.conflictDetails ? (bridge.conflictDetails.sourceMtimeStr || "") : ""
                                color: Qt.rgba(1, 1, 1, 0.45)
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                width: parent.width
                            }
                        }
                    }
                }

                // "Apply to all" checkbox (disabled if type mismatch)
                Row {
                    spacing: 8
                    visible: !(typeof bridge !== "undefined" && bridge.conflictDetails && Boolean(bridge.conflictDetails.isTypeMismatch))

                    Rectangle {
                        width: 16
                        height: 16
                        radius: 4
                        color: (typeof bridge !== "undefined" && bridge.applyToAll) ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.1)
                        border.color: (typeof bridge !== "undefined" && bridge.applyToAll) ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.3)
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        TinexusVectorIcon {
                            anchors.centerIn: parent
                            name: "check"
                            size: 11
                            color: "#FFFFFF"
                            visible: typeof bridge !== "undefined" && bridge.applyToAll
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    bridge.applyToAll = !bridge.applyToAll;
                                }
                            }
                        }
                    }

                    Text {
                        text: "Apply to all conflicts in this operation"
                        color: Qt.rgba(1, 1, 1, 0.75)
                        font.pixelSize: 12
                        anchors.verticalCenter: parent.verticalCenter

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    bridge.applyToAll = !bridge.applyToAll;
                                }
                            }
                        }
                    }
                }

                // Action Buttons Row: Skip, Keep Both, Replace
                Row {
                    width: parent.width
                    spacing: 10

                    Item {
                        width: parent.width - (skipBtn.width + keepBtn.width + replaceBtn.width + 20)
                    }

                    // Skip Button
                    Rectangle {
                        id: skipBtn
                        width: 80
                        height: 32
                        radius: 6
                        color: skipMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.08)
                        border.color: Qt.rgba(1, 1, 1, 0.15)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Skip"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            id: skipMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    bridge.resolveConflict(0, bridge.applyToAll); // 0 = Skip
                                }
                            }
                        }
                    }

                    // Keep Both Button
                    Rectangle {
                        id: keepBtn
                        width: 96
                        height: 32
                        radius: 6
                        color: keepMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.08)
                        border.color: Qt.rgba(1, 1, 1, 0.15)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Keep Both"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            id: keepMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    bridge.resolveConflict(2, bridge.applyToAll); // 2 = KeepBoth
                                }
                            }
                        }
                    }

                    // Replace Button
                    Rectangle {
                        id: replaceBtn
                        width: 86
                        height: 32
                        radius: 6
                        color: replaceMouse.containsMouse ? "#0071E3" : "#0A84FF"

                        Text {
                            anchors.centerIn: parent
                            text: "Replace"
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            id: replaceMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    bridge.resolveConflict(1, bridge.applyToAll); // 1 = Replace
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

