// ============================================================================
// MonitorWindow.qml — Qt6 / QML Tinexus Activity Monitor
// Real-time Linux sysfs/procfs Telemetry & Process Management
// macOS Activity Monitor obsidian aesthetics, 100% Genuine Probes
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
    minimumWidth: 740
    minimumHeight: 460
    color: "#14151B"
    flags: Qt.Window | Qt.FramelessWindowHint
    visibility: Window.Windowed
    title: "Activity Monitor"

    property int activeTab: 0 // 0: Processes, 1: Performance, 2: Services

    // Strict fixed-column geometry for Activity Monitor table
    readonly property int colWidthName: 230
    readonly property int colWidthPid: 65
    readonly property int colWidthUser: 80
    readonly property int colWidthCpu: 75
    readonly property int colWidthMem: 85
    readonly property int colWidthDisk: 95
    readonly property int colSpacing: 8

    onActiveTabChanged: {
        if (activeTab === 1) {
            Qt.callLater(function() {
                if (typeof cpuCanvas !== "undefined") cpuCanvas.requestPaint();
                if (typeof memCanvas !== "undefined") memCanvas.requestPaint();
                if (typeof diskCanvas !== "undefined") diskCanvas.requestPaint();
                if (typeof netCanvas !== "undefined") netCanvas.requestPaint();
            });
        }
    }

    // Live auto-refresh timer (1.5s interval)
    Timer {
        interval: 1500
        running: true
        repeat: true
        onTriggered: monitorBridge.refresh()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Top Titlebar & Tab Segment Control ──────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: "#1A1D27"
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            z: 10

            MouseArea {
                anchors.fill: parent
                onPressed: rootWindow.startSystemMove()
            }

            // Left: Traffic lights and title
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 12

                MacTrafficLights {
                    targetWindow: rootWindow
                    anchors.verticalCenter: parent.verticalCenter
                }

                Item { width: 4; height: 1 }

                Text {
                    text: "Activity Monitor"
                    color: "#F3F4F6"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // Center: Segmented Tab Switcher (Mathematically centered, zero collision)
            Rectangle {
                anchors.centerIn: parent
                height: 30
                width: 320
                radius: 7
                color: Qt.rgba(0, 0, 0, 0.4)
                border.color: Qt.rgba(1, 1, 1, 0.08)
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 2
                    spacing: 2

                    Repeater {
                        model: [
                            { label: "Processes", tab: 0 },
                            { label: "Performance", tab: 1 },
                            { label: "Services", tab: 2 }
                        ]

                        Rectangle {
                            width: (parent.width - 4) / 3
                            height: parent.height
                            radius: 5
                            color: rootWindow.activeTab === modelData.tab ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                color: rootWindow.activeTab === modelData.tab ? "#FFFFFF" : "#9CA3AF"
                                font.pixelSize: 12
                                font.weight: rootWindow.activeTab === modelData.tab ? Font.Medium : Font.Normal
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: rootWindow.activeTab = modelData.tab
                            }
                        }
                    }
                }
            }

            // Right: Live Pulse Indicator & Manual Refresh
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: "#10B981"
                    anchors.verticalCenter: parent.verticalCenter

                    SequentialAnimation on opacity {
                        loops: Animation.Infinite
                        PropertyAnimation { from: 0.4; to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                        PropertyAnimation { from: 1.0; to: 0.4; duration: 800; easing.type: Easing.InOutQuad }
                    }
                }

                Rectangle {
                    width: 26
                    height: 26
                    radius: 6
                    color: refreshMa.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"
                    anchors.verticalCenter: parent.verticalCenter

                    Text {
                        anchors.centerIn: parent
                        text: "↻"
                        color: "#9CA3AF"
                        font.pixelSize: 14
                    }

                    MouseArea {
                        id: refreshMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: monitorBridge.refresh()
                    }
                }
            }
        }

        // ── Tab 0: Processes ────────────────────────────────────────
        Item {
            id: processesView
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: rootWindow.activeTab === 0

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                // Filter & Action Toolbar
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    // Search Box
                    Rectangle {
                        Layout.preferredWidth: 260
                        Layout.preferredHeight: 30
                        radius: 7
                        color: Qt.rgba(0, 0, 0, 0.35)
                        border.color: Qt.rgba(1, 1, 1, 0.1)
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            Text {
                                text: "⌕"
                                color: "#6B7280"
                                font.pixelSize: 14
                            }

                            TextInput {
                                id: searchInput
                                Layout.fillWidth: true
                                color: "#F3F4F6"
                                font.pixelSize: 12
                                verticalAlignment: TextInput.AlignVCenter
                                clip: true

                                Text {
                                    anchors.fill: parent
                                    text: "Search processes (PID or Name)..."
                                    color: "#6B7280"
                                    font.pixelSize: 12
                                    verticalAlignment: Text.AlignVCenter
                                    visible: !searchInput.text && !searchInput.activeFocus
                                }

                                onTextChanged: monitorBridge.searchQuery = text
                            }

                            Text {
                                text: "✕"
                                color: "#9CA3AF"
                                font.pixelSize: 11
                                visible: searchInput.text.length > 0
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: searchInput.text = ""
                                }
                            }
                        }
                    }

                    // Category Pill Tabs
                    Rectangle {
                        Layout.preferredWidth: 240
                        Layout.preferredHeight: 30
                        radius: 7
                        color: Qt.rgba(0, 0, 0, 0.35)
                        border.color: Qt.rgba(1, 1, 1, 0.1)
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 2

                            Repeater {
                                model: [
                                    { label: "All", cat: 0 },
                                    { label: "My Procs", cat: 1 },
                                    { label: "System", cat: 2 }
                                ]

                                Rectangle {
                                    width: (parent.width - 4) / 3
                                    height: parent.height
                                    radius: 5
                                    color: monitorBridge.category === modelData.cat ? Qt.rgba(1, 1, 1, 0.15) : "transparent"

                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: monitorBridge.category === modelData.cat ? "#FFFFFF" : "#9CA3AF"
                                        font.pixelSize: 11
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: monitorBridge.category = modelData.cat
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillWidth: true }

                    // Process Count Label
                    Text {
                        text: monitorBridge.processCount + " Processes"
                        color: "#9CA3AF"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item { Layout.preferredWidth: 6 }

                    // End Process Button
                    Rectangle {
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 28
                        radius: 6
                        color: monitorBridge.selectedPid > 0 ? (endMa.containsMouse ? "#2A2E3D" : "#1F2330") : "#161822"
                        border.color: monitorBridge.selectedPid > 0 ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(1, 1, 1, 0.05)
                        border.width: 1
                        opacity: monitorBridge.selectedPid > 0 ? 1.0 : 0.4

                        Text {
                            anchors.centerIn: parent
                            text: "Quit Process"
                            color: "#E5E7EB"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }

                        MouseArea {
                            id: endMa
                            anchors.fill: parent
                            enabled: monitorBridge.selectedPid > 0
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: monitorBridge.openKillDialog(false)
                        }
                    }

                    // Force Quit Button
                    Rectangle {
                        Layout.preferredWidth: 90
                        Layout.preferredHeight: 28
                        radius: 6
                        color: monitorBridge.selectedPid > 0 ? (forceMa.containsMouse ? "#991B1B" : "#7F1D1D") : "#161822"
                        border.color: monitorBridge.selectedPid > 0 ? "#EF4444" : Qt.rgba(1, 1, 1, 0.05)
                        border.width: 1
                        opacity: monitorBridge.selectedPid > 0 ? 1.0 : 0.4

                        Text {
                            anchors.centerIn: parent
                            text: "Force Quit"
                            color: "#FCA5A5"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }

                        MouseArea {
                            id: forceMa
                            anchors.fill: parent
                            enabled: monitorBridge.selectedPid > 0
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: monitorBridge.openKillDialog(true)
                        }
                    }
                }

                // Process Table Card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#181B24"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Table Header
                        Rectangle {
                            Layout.fillWidth: true
                            height: 30
                            color: "#1F2330"
                            border.color: Qt.rgba(1, 1, 1, 0.06)
                            border.width: 1

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: rootWindow.colSpacing

                                // Helper function for sort header click
                                Component {
                                    id: headerColComp
                                    Item {
                                        property string title
                                        property string colKey
                                        property int fixedWidth
                                        width: fixedWidth
                                        height: parent.height

                                        Row {
                                            anchors.fill: parent
                                            spacing: 4

                                            Text {
                                                text: title
                                                color: monitorBridge.sortColumn === colKey ? "#38BDF8" : "#9CA3AF"
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                                anchors.verticalCenter: parent.verticalCenter
                                            }

                                            Text {
                                                text: monitorBridge.sortColumn === colKey ? (monitorBridge.sortAscending ? "▲" : "▼") : ""
                                                color: "#38BDF8"
                                                font.pixelSize: 9
                                                anchors.verticalCenter: parent.verticalCenter
                                                visible: monitorBridge.sortColumn === colKey
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: monitorBridge.toggleSort(colKey)
                                        }
                                    }
                                }

                                Loader {
                                    width: rootWindow.colWidthName; height: parent.height
                                    sourceComponent: headerColComp
                                    onLoaded: { item.title = "Process Name"; item.colKey = "name"; item.fixedWidth = rootWindow.colWidthName; }
                                }
                                Loader {
                                    width: rootWindow.colWidthPid; height: parent.height
                                    sourceComponent: headerColComp
                                    onLoaded: { item.title = "PID"; item.colKey = "pid"; item.fixedWidth = rootWindow.colWidthPid; }
                                }
                                Loader {
                                    width: rootWindow.colWidthUser; height: parent.height
                                    sourceComponent: headerColComp
                                    onLoaded: { item.title = "User"; item.colKey = "user"; item.fixedWidth = rootWindow.colWidthUser; }
                                }
                                Loader {
                                    width: rootWindow.colWidthCpu; height: parent.height
                                    sourceComponent: headerColComp
                                    onLoaded: { item.title = "% CPU"; item.colKey = "cpu"; item.fixedWidth = rootWindow.colWidthCpu; }
                                }
                                Loader {
                                    width: rootWindow.colWidthMem; height: parent.height
                                    sourceComponent: headerColComp
                                    onLoaded: { item.title = "Memory"; item.colKey = "memory"; item.fixedWidth = rootWindow.colWidthMem; }
                                }
                                Loader {
                                    width: rootWindow.colWidthDisk; height: parent.height
                                    sourceComponent: headerColComp
                                    onLoaded: { item.title = "Disk I/O"; item.colKey = "disk"; item.fixedWidth = rootWindow.colWidthDisk; }
                                }
                                Loader {
                                    width: Math.max(50, parent.width - (rootWindow.colWidthName + rootWindow.colWidthPid + rootWindow.colWidthUser + rootWindow.colWidthCpu + rootWindow.colWidthMem + rootWindow.colWidthDisk + rootWindow.colSpacing * 6))
                                    height: parent.height
                                    sourceComponent: headerColComp
                                    onLoaded: { item.title = "State"; item.colKey = "state"; item.fixedWidth = parent.width; }
                                }
                            }
                        }

                        // Table Rows ListView
                        ListView {
                            id: procList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: monitorBridge.processes
                            boundsBehavior: Flickable.StopAtBounds

                            property real savedContentY: 0
                            onContentYChanged: {
                                if (moving || dragging || flicking) {
                                    savedContentY = contentY;
                                }
                            }

                            Connections {
                                target: monitorBridge
                                function onProcessesChanged() {
                                    if (procList.savedContentY > 0 && !(procList.moving || procList.dragging || procList.flicking)) {
                                        var targetY = procList.savedContentY;
                                        Qt.callLater(function() {
                                            if (procList.contentHeight > procList.height) {
                                                procList.contentY = Math.min(targetY, procList.contentHeight - procList.height);
                                            }
                                        });
                                    }
                                }
                            }

                            delegate: Rectangle {
                                width: procList.width
                                height: 28
                                color: {
                                    if (modelData.pid === monitorBridge.selectedPid) return "#0A84FF"
                                    if (rowMa.containsMouse) return "#222634"
                                    return index % 2 === 0 ? "#181B24" : "#15171F"
                                }

                                MouseArea {
                                    id: rowMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: monitorBridge.selectProcess(modelData.pid)
                                    onDoubleClicked: monitorBridge.openKillDialog(false)
                                }

                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: rootWindow.colSpacing

                                    // Process Name + Protected Vector Badge
                                    Item {
                                        width: rootWindow.colWidthName
                                        height: parent.height
                                        clip: true

                                        Row {
                                            anchors.fill: parent
                                            spacing: 6

                                            Rectangle {
                                                width: 14; height: 14; radius: 3
                                                color: modelData.isProtected ? "#F59E0B" : (modelData.isSystem ? "#6366F1" : "#0A84FF")
                                                opacity: 0.9
                                                anchors.verticalCenter: parent.verticalCenter

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: modelData.isProtected ? "P" : (modelData.isSystem ? "S" : "A")
                                                    color: "#FFFFFF"
                                                    font.pixelSize: 9
                                                    font.bold: true
                                                }
                                            }

                                            Text {
                                                width: parent.width - 24
                                                text: modelData.name
                                                color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#F3F4F6"
                                                font.pixelSize: 12
                                                font.weight: modelData.pid === monitorBridge.selectedPid ? Font.Medium : Font.Normal
                                                elide: Text.ElideRight
                                                anchors.verticalCenter: parent.verticalCenter
                                            }
                                        }
                                    }

                                    // PID
                                    Text {
                                        width: rootWindow.colWidthPid
                                        text: modelData.pid
                                        color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#9CA3AF"
                                        font.pixelSize: 11
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    // User
                                    Text {
                                        width: rootWindow.colWidthUser
                                        text: modelData.user
                                        color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#9CA3AF"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    // CPU %
                                    Text {
                                        width: rootWindow.colWidthCpu
                                        text: modelData.cpuStr
                                        color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : (modelData.cpu > 10.0 ? "#F87171" : "#E5E7EB")
                                        font.pixelSize: 11
                                        font.weight: modelData.cpu > 5.0 ? Font.Medium : Font.Normal
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    // Memory
                                    Text {
                                        width: rootWindow.colWidthMem
                                        text: modelData.rssStr
                                        color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#E5E7EB"
                                        font.pixelSize: 11
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    // Disk I/O
                                    Text {
                                        width: rootWindow.colWidthDisk
                                        text: modelData.diskStr
                                        color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#9CA3AF"
                                        font.pixelSize: 11
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    // State
                                    Text {
                                        width: Math.max(50, parent.width - (rootWindow.colWidthName + rootWindow.colWidthPid + rootWindow.colWidthUser + rootWindow.colWidthCpu + rootWindow.colWidthMem + rootWindow.colWidthDisk + rootWindow.colSpacing * 6))
                                        text: modelData.state
                                        color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#9CA3AF"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── Tab 1: Performance ──────────────────────────────────────
        Item {
            id: performanceView
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: rootWindow.activeTab === 1

            GridLayout {
                anchors.fill: parent
                anchors.margins: 16
                columns: 2
                rowSpacing: 14
                columnSpacing: 14

                // 1. CPU Card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#181B24"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Processor (CPU)"
                                color: "#F3F4F6"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: monitorBridge.cpuUsage.toFixed(1) + "%"
                                color: "#38BDF8"
                                font.pixelSize: 15
                                font.weight: Font.Bold
                            }
                        }

                        Text {
                            text: monitorBridge.cpuModel + " (" + monitorBridge.cpuCoresCount + " Cores)"
                            color: "#9CA3AF"
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        // Canvas Graph for CPU
                        Canvas {
                            id: cpuCanvas
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width, h = height;
                                ctx.fillStyle = "#12141C";
                                ctx.fillRect(0, 0, w, h);

                                var history = monitorBridge.cpuHistory;
                                if (history.length < 2) return;

                                var step = w / 59.0;
                                ctx.beginPath();
                                ctx.moveTo(0, h);

                                for (var i = 0; i < history.length; ++i) {
                                    var val = Math.min(100.0, Math.max(0.0, history[i]));
                                    var x = (60 - history.length + i) * step;
                                    var y = h - (val / 100.0) * h;
                                    if (i === 0) ctx.lineTo(x, y);
                                    else ctx.lineTo(x, y);
                                }
                                ctx.lineTo(w, h);
                                ctx.closePath();

                                var grad = ctx.createLinearGradient(0, 0, 0, h);
                                grad.addColorStop(0, "rgba(56, 189, 248, 0.35)");
                                grad.addColorStop(1, "rgba(56, 189, 248, 0.02)");
                                ctx.fillStyle = grad;
                                ctx.fill();

                                // Top stroke
                                ctx.beginPath();
                                for (var j = 0; j < history.length; ++j) {
                                    var v = Math.min(100.0, Math.max(0.0, history[j]));
                                    var px = (60 - history.length + j) * step;
                                    var py = h - (v / 100.0) * h;
                                    if (j === 0) ctx.moveTo(px, py);
                                    else ctx.lineTo(px, py);
                                }
                                ctx.strokeStyle = "#38BDF8";
                                ctx.lineWidth = 2;
                                ctx.stroke();
                            }
                            Connections {
                                target: monitorBridge
                                function onHistoryChanged() {
                                    if (rootWindow.activeTab === 1) cpuCanvas.requestPaint();
                                }
                            }
                        }
                    }
                }

                // 2. Memory Card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#181B24"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "System Memory (RAM)"
                                color: "#F3F4F6"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: monitorBridge.memUsedGb + " / " + monitorBridge.memTotalGb + " (" + monitorBridge.memUsagePct.toFixed(0) + "%)"
                                color: "#A855F7"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                            }
                        }

                        // Gauge Bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 6
                            radius: 3
                            color: "#222634"

                            Rectangle {
                                width: parent.width * (monitorBridge.memUsagePct / 100.0)
                                height: parent.height
                                radius: 3
                                color: "#A855F7"
                            }
                        }

                        // Canvas Graph for Memory
                        Canvas {
                            id: memCanvas
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width, h = height;
                                ctx.fillStyle = "#12141C";
                                ctx.fillRect(0, 0, w, h);

                                var history = monitorBridge.memHistory;
                                if (history.length < 2) return;

                                var step = w / 59.0;
                                ctx.beginPath();
                                ctx.moveTo(0, h);

                                for (var i = 0; i < history.length; ++i) {
                                    var val = Math.min(100.0, Math.max(0.0, history[i]));
                                    var x = (60 - history.length + i) * step;
                                    var y = h - (val / 100.0) * h;
                                    if (i === 0) ctx.lineTo(x, y);
                                    else ctx.lineTo(x, y);
                                }
                                ctx.lineTo(w, h);
                                ctx.closePath();

                                var grad = ctx.createLinearGradient(0, 0, 0, h);
                                grad.addColorStop(0, "rgba(168, 85, 247, 0.35)");
                                grad.addColorStop(1, "rgba(168, 85, 247, 0.02)");
                                ctx.fillStyle = grad;
                                ctx.fill();

                                ctx.beginPath();
                                for (var j = 0; j < history.length; ++j) {
                                    var v = Math.min(100.0, Math.max(0.0, history[j]));
                                    var px = (60 - history.length + j) * step;
                                    var py = h - (v / 100.0) * h;
                                    if (j === 0) ctx.moveTo(px, py);
                                    else ctx.lineTo(px, py);
                                }
                                ctx.strokeStyle = "#A855F7";
                                ctx.lineWidth = 2;
                                ctx.stroke();
                            }
                            Connections {
                                target: monitorBridge
                                function onHistoryChanged() {
                                    if (rootWindow.activeTab === 1) memCanvas.requestPaint();
                                }
                            }
                        }
                    }
                }

                // 3. Disk I/O Card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#181B24"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Disk Throughput (I/O)"
                                color: "#F3F4F6"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "R: " + monitorBridge.diskReadRate + "  W: " + monitorBridge.diskWriteRate
                                color: "#10B981"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                            }
                        }

                        Text {
                            text: "Storage Controller Activity"
                            color: "#9CA3AF"
                            font.pixelSize: 11
                        }

                        Canvas {
                            id: diskCanvas
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width, h = height;
                                ctx.fillStyle = "#12141C";
                                ctx.fillRect(0, 0, w, h);

                                var history = monitorBridge.diskHistory;
                                if (history.length < 2) return;

                                var maxVal = 500.0;
                                for (var m = 0; m < history.length; ++m) {
                                    if (history[m] > maxVal) maxVal = history[m] * 1.2;
                                }

                                var step = w / 59.0;
                                ctx.beginPath();
                                ctx.moveTo(0, h);

                                for (var i = 0; i < history.length; ++i) {
                                    var val = Math.max(0.0, history[i]);
                                    var x = (60 - history.length + i) * step;
                                    var y = h - (val / maxVal) * h;
                                    if (i === 0) ctx.lineTo(x, y);
                                    else ctx.lineTo(x, y);
                                }
                                ctx.lineTo(w, h);
                                ctx.closePath();

                                var grad = ctx.createLinearGradient(0, 0, 0, h);
                                grad.addColorStop(0, "rgba(16, 185, 129, 0.35)");
                                grad.addColorStop(1, "rgba(16, 185, 129, 0.02)");
                                ctx.fillStyle = grad;
                                ctx.fill();

                                ctx.beginPath();
                                for (var j = 0; j < history.length; ++j) {
                                    var v = Math.max(0.0, history[j]);
                                    var px = (60 - history.length + j) * step;
                                    var py = h - (v / maxVal) * h;
                                    if (j === 0) ctx.moveTo(px, py);
                                    else ctx.lineTo(px, py);
                                }
                                ctx.strokeStyle = "#10B981";
                                ctx.lineWidth = 2;
                                ctx.stroke();
                            }
                            Connections {
                                target: monitorBridge
                                function onHistoryChanged() {
                                    if (rootWindow.activeTab === 1) diskCanvas.requestPaint();
                                }
                            }
                        }
                    }
                }

                // 4. Network Card
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#181B24"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Network Traffic"
                                color: "#F3F4F6"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "↓ " + monitorBridge.netRxRate + "   ↑ " + monitorBridge.netTxRate
                                color: "#F59E0B"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                            }
                        }

                        Text {
                            text: "Total Bandwidth Across Active Network Interfaces"
                            color: "#9CA3AF"
                            font.pixelSize: 11
                        }

                        Canvas {
                            id: netCanvas
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            onPaint: {
                                var ctx = getContext("2d");
                                ctx.reset();
                                var w = width, h = height;
                                ctx.fillStyle = "#12141C";
                                ctx.fillRect(0, 0, w, h);

                                var history = monitorBridge.netHistory;
                                if (history.length < 2) return;

                                var maxVal = 500.0;
                                for (var m = 0; m < history.length; ++m) {
                                    if (history[m] > maxVal) maxVal = history[m] * 1.2;
                                }

                                var step = w / 59.0;
                                ctx.beginPath();
                                ctx.moveTo(0, h);

                                for (var i = 0; i < history.length; ++i) {
                                    var val = Math.max(0.0, history[i]);
                                    var x = (60 - history.length + i) * step;
                                    var y = h - (val / maxVal) * h;
                                    if (i === 0) ctx.lineTo(x, y);
                                    else ctx.lineTo(x, y);
                                }
                                ctx.lineTo(w, h);
                                ctx.closePath();

                                var grad = ctx.createLinearGradient(0, 0, 0, h);
                                grad.addColorStop(0, "rgba(245, 158, 11, 0.35)");
                                grad.addColorStop(1, "rgba(245, 158, 11, 0.02)");
                                ctx.fillStyle = grad;
                                ctx.fill();

                                ctx.beginPath();
                                for (var j = 0; j < history.length; ++j) {
                                    var v = Math.max(0.0, history[j]);
                                    var px = (60 - history.length + j) * step;
                                    var py = h - (v / maxVal) * h;
                                    if (j === 0) ctx.moveTo(px, py);
                                    else ctx.lineTo(px, py);
                                }
                                ctx.strokeStyle = "#F59E0B";
                                ctx.lineWidth = 2;
                                ctx.stroke();
                            }
                            Connections {
                                target: monitorBridge
                                function onHistoryChanged() {
                                    if (rootWindow.activeTab === 1) netCanvas.requestPaint();
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── Tab 2: Services ─────────────────────────────────────────
        Item {
            id: servicesView
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: rootWindow.activeTab === 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Supervised Platform Services (tinexus-serviced Tree)"
                        color: "#F3F4F6"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: monitorBridge.services.length + " Supervised Daemons"
                        color: "#9CA3AF"
                        font.pixelSize: 12
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#181B24"
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Header
                        Rectangle {
                            Layout.fillWidth: true
                            height: 30
                            color: "#1F2330"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8

                                Text { Layout.preferredWidth: 180; text: "Service Name"; color: "#9CA3AF"; font.pixelSize: 11; font.weight: Font.DemiBold }
                                Text { Layout.fillWidth: true; text: "Role & Architecture Layer"; color: "#9CA3AF"; font.pixelSize: 11; font.weight: Font.DemiBold }
                                Text { Layout.preferredWidth: 100; text: "Process ID"; color: "#9CA3AF"; font.pixelSize: 11; font.weight: Font.DemiBold }
                                Text { Layout.preferredWidth: 90; text: "Status"; color: "#9CA3AF"; font.pixelSize: 11; font.weight: Font.DemiBold }
                                Text { Layout.preferredWidth: 90; text: "Action"; color: "#9CA3AF"; font.pixelSize: 11; font.weight: Font.DemiBold }
                            }
                        }

                        ListView {
                            id: svcsList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: monitorBridge.services

                            delegate: Rectangle {
                                width: svcsList.width
                                height: 36
                                color: index % 2 === 0 ? "#181B24" : "#15171F"

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8

                                    // Name
                                    RowLayout {
                                        Layout.preferredWidth: 180
                                        spacing: 6
                                        Text {
                                            text: modelData.isProtected ? "🛡" : "⚙"
                                            color: modelData.isProtected ? "#38BDF8" : "#9CA3AF"
                                            font.pixelSize: 12
                                        }
                                        Text {
                                            text: modelData.name
                                            color: "#F3F4F6"
                                            font.pixelSize: 12
                                            font.weight: Font.Medium
                                        }
                                    }

                                    // Role
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.role
                                        color: "#9CA3AF"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }

                                    // PID
                                    Text {
                                        Layout.preferredWidth: 100
                                        text: modelData.pidStr
                                        color: modelData.active ? "#38BDF8" : "#6B7280"
                                        font.pixelSize: 11
                                    }

                                    // Status Badge
                                    Rectangle {
                                        Layout.preferredWidth: 80
                                        Layout.preferredHeight: 20
                                        radius: 10
                                        color: modelData.active ? Qt.rgba(0.06, 0.73, 0.51, 0.15) : Qt.rgba(0.96, 0.62, 0.04, 0.15)
                                        border.color: modelData.active ? "#10B981" : "#F59E0B"
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.status
                                            color: modelData.active ? "#10B981" : "#F59E0B"
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                        }
                                    }

                                    // Restart Action Button
                                    Rectangle {
                                        Layout.preferredWidth: 80
                                        Layout.preferredHeight: 22
                                        radius: 4
                                        color: modelData.isProtected ? "#161822" : (rstMa.containsMouse ? "#2A2E3D" : "#1F2330")
                                        border.color: modelData.isProtected ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.15)
                                        border.width: 1
                                        opacity: modelData.isProtected ? 0.4 : 1.0

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.isProtected ? "Protected" : "Restart"
                                            color: modelData.isProtected ? "#6B7280" : "#E5E7EB"
                                            font.pixelSize: 10
                                            font.weight: Font.Medium
                                        }

                                        MouseArea {
                                            id: rstMa
                                            anchors.fill: parent
                                            enabled: !modelData.isProtected
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: monitorBridge.restartService(modelData.name)
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

    // ── Safety Confirmation Modal Sheet ─────────────────────────────
    Rectangle {
        id: killModal
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.65)
        visible: monitorBridge.killModalOpen
        z: 100

        MouseArea {
            anchors.fill: parent
            // Eat clicks outside dialog
        }

        Rectangle {
            anchors.centerIn: parent
            width: 440
            height: monitorBridge.selectedIsProtected ? 220 : 190
            radius: 12
            color: "#1C1F2B"
            border.color: monitorBridge.selectedIsProtected ? "#EF4444" : Qt.rgba(1, 1, 1, 0.15)
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 12

                RowLayout {
                    spacing: 12
                    Text {
                        text: monitorBridge.selectedIsProtected ? "🛑" : "⚠️"
                        font.pixelSize: 28
                        Layout.alignment: Qt.AlignTop
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Text {
                            text: monitorBridge.killModalForce ? "Force Quit Process?" : "Quit Process?"
                            color: "#F3F4F6"
                            font.pixelSize: 15
                            font.weight: Font.Bold
                        }

                        Text {
                            text: "Do you want to " + (monitorBridge.killModalForce ? "force quit" : "quit") + " \"" + monitorBridge.selectedProcessName + "\" (PID " + monitorBridge.selectedPid + ")?"
                            color: "#D1D5DB"
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }

                // Protected warning if user selected critical daemon
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    radius: 6
                    color: Qt.rgba(0.94, 0.27, 0.27, 0.15)
                    border.color: "#EF4444"
                    border.width: 1
                    visible: monitorBridge.selectedIsProtected

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6
                        Text { text: "⚠️"; font.pixelSize: 14 }
                        Text {
                            text: "Protected system process. Action blocked for stability."
                            color: "#FCA5A5"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                // Dialog Buttons
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Item { Layout.fillWidth: true }

                    Rectangle {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 30
                        radius: 6
                        color: cancelMa.containsMouse ? "#2A2E3D" : "#222634"
                        border.color: Qt.rgba(1, 1, 1, 0.15)
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Cancel"
                            color: "#E5E7EB"
                            font.pixelSize: 12
                        }

                        MouseArea {
                            id: cancelMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: monitorBridge.closeKillDialog()
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: monitorBridge.killModalForce ? 100 : 90
                        Layout.preferredHeight: 30
                        radius: 6
                        color: monitorBridge.selectedIsProtected ? "#374151" : (actionMa.containsMouse ? "#B91C1C" : "#DC2626")
                        opacity: monitorBridge.selectedIsProtected ? 0.4 : 1.0

                        Text {
                            anchors.centerIn: parent
                            text: monitorBridge.selectedIsProtected ? "Blocked" : (monitorBridge.killModalForce ? "Force Quit" : "Quit")
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            id: actionMa
                            anchors.fill: parent
                            enabled: !monitorBridge.selectedIsProtected
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: monitorBridge.confirmKillProcess()
                        }
                    }
                }
            }
        }
    }
}
