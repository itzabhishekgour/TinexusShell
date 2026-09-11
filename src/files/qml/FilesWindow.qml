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
                        Text { anchors.centerIn: parent; text: "◀"; color: (typeof bridge !== "undefined" && bridge.canGoBack) ? "#FFFFFF" : Qt.rgba(1,1,1,0.3); font.pixelSize: 11 }
                        MouseArea {
                            id: bMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            enabled: typeof bridge !== "undefined" && bridge.canGoBack
                            onClicked: bridge.goBack()
                        }
                    }

                    Rectangle {
                        width: 28; height: 28; radius: 6
                        color: (typeof bridge !== "undefined" && bridge.canGoForward) ? (fMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.1)) : Qt.rgba(1,1,1,0.04)
                        Text { anchors.centerIn: parent; text: "▶"; color: (typeof bridge !== "undefined" && bridge.canGoForward) ? "#FFFFFF" : Qt.rgba(1,1,1,0.3); font.pixelSize: 11 }
                        MouseArea {
                            id: fMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                            enabled: typeof bridge !== "undefined" && bridge.canGoForward
                            onClicked: bridge.goForward()
                        }
                    }

                    Rectangle {
                        width: 28; height: 28; radius: 6
                        color: uMouse.containsMouse ? Qt.rgba(1,1,1,0.2) : Qt.rgba(1,1,1,0.1)
                        Text { anchors.centerIn: parent; text: "▲"; color: "#FFFFFF"; font.pixelSize: 11 }
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
                                { label: "⊞", mode: 0, tip: "Icons" },
                                { label: "☰", mode: 1, tip: "List" },
                                { label: "▥", mode: 2, tip: "Columns" },
                                { label: "⧉", mode: 3, tip: "Gallery" }
                            ]

                            delegate: Rectangle {
                                width: 36
                                height: 28
                                radius: 6
                                readonly property bool isActive: typeof bridge !== "undefined" && bridge.currentViewMode === modelData.mode
                                color: isActive ? "#0A84FF" : (segMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.10) : "transparent")

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: isActive ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.75)
                                    font.pixelSize: 13
                                    font.bold: isActive
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

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        color: "#FFFFFF"
                        font.pixelSize: 15
                        font.bold: true
                    }

                    MouseArea {
                        id: newFMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: rootWindow.newFolderDialogVisible = true
                    }
                }

                // Path Breadcrumb Box
                Rectangle {
                    Layout.fillWidth: true
                    height: 28
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.35)
                    border.color: Qt.rgba(1, 1, 1, 0.10)
                    border.width: 1
                    Layout.alignment: Qt.AlignVCenter

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 8

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

                        Text {
                            text: "↻"
                            color: refMouse.containsMouse ? "#3B82F6" : Qt.rgba(1, 1, 1, 0.6)
                            font.pixelSize: 14
                            Layout.alignment: Qt.AlignVCenter
                            MouseArea {
                                id: refMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onClicked: { if (typeof bridge !== "undefined") bridge.refresh() }
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
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton | Qt.LeftButton
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
                            readonly property bool isSelected: typeof bridge !== "undefined" && bridge.selectedPath === modelData.path
                            color: isSelected ? Qt.rgba(0.04, 0.52, 1.0, 0.35) : (itemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
                            border.color: isSelected ? "#0A84FF" : (itemMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent")
                            border.width: 1

                            Column {
                                anchors.centerIn: parent
                                spacing: 6
                                width: 90

                                FileVectorIcon {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    iconKind: modelData.iconKind
                                    isDir: modelData.isDir
                                    iconSize: 44
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
                                    if (typeof bridge !== "undefined") bridge.selectItem(modelData.path);
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

                    // Table Column Headers
                    Rectangle {
                        id: listHeader
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 28
                        color: "#1E222E"
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 10

                            Text { text: "Name"; color: "#86868B"; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 320 }
                            Text { text: "Date Modified"; color: "#86868B"; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 160 }
                            Text { text: "Size"; color: "#86868B"; font.pixelSize: 11; font.bold: true; Layout.preferredWidth: 90 }
                            Text { text: "Kind"; color: "#86868B"; font.pixelSize: 11; font.bold: true; Layout.fillWidth: true }
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
                                readonly property bool isSelected: typeof bridge !== "undefined" && bridge.selectedPath === modelData.path
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
                                            text: modelData.name
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
                                        if (typeof bridge !== "undefined") bridge.selectItem(modelData.path);
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

                                                Text {
                                                    visible: modelData.isDir
                                                    text: "›"
                                                    color: isSelected ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.35)
                                                    font.pixelSize: 13
                                                    font.bold: true
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
                                    readonly property bool isSelected: typeof bridge !== "undefined" && bridge.selectedPath === modelData.path
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
                                        onClicked: {
                                            if (typeof bridge !== "undefined") {
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
                Text { text: "Copy"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmCopyMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { rootWindow.contextMenuVisible = false; if (typeof bridge !== "undefined") bridge.copyItem(rootWindow.contextMenuTarget); }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg
                color: itmCutMouse.containsMouse ? "#0A84FF" : "transparent"
                Text { text: "Cut"; color: "#FFFFFF"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmCutMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { rootWindow.contextMenuVisible = false; if (typeof bridge !== "undefined") bridge.cutItem(rootWindow.contextMenuTarget); }
                }
            }
            Rectangle {
                width: parent.width; height: 26; radius: 5
                visible: !rootWindow.contextMenuIsBg
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
                Text { text: "Move to Trash"; color: itmDelMouse.containsMouse ? "#FFFFFF" : "#FF453A"; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter; anchors.left: parent.left; anchors.leftMargin: 10 }
                MouseArea {
                    id: itmDelMouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                    onClicked: { rootWindow.contextMenuVisible = false; if (typeof bridge !== "undefined") bridge.deleteItem(rootWindow.contextMenuTarget); }
                }
            }

            // Color Tags Divider & Row
            Rectangle {
                width: parent.width - 12; height: 1
                anchors.horizontalCenter: parent.horizontalCenter
                color: Qt.rgba(1, 1, 1, 0.1)
                visible: !rootWindow.contextMenuIsBg
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6
                visible: !rootWindow.contextMenuIsBg
                height: 22

                Repeater {
                    model: ["#FF453A", "#FF9F0A", "#FFD60A", "#30D158", "#0A84FF", "#BF5AF2", "#8E8E93"]
                    delegate: Rectangle {
                        width: 14; height: 14; radius: 7
                        color: modelData
                        anchors.verticalCenter: parent.verticalCenter
                        border.color: tagMouse.containsMouse ? "#FFFFFF" : "transparent"
                        border.width: 1.5

                        MouseArea {
                            id: tagMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                rootWindow.contextMenuVisible = false;
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
}
