// ============================================================================
// MonitorWindow.qml — Qt6 / QML Tinexus Activity Monitor
// Real-time Linux sysfs/procfs Telemetry & Process Management
// ============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../../common/qml"

Window {
    id: rootWindow
    width: 920
    height: 600
    minimumWidth: 780
    minimumHeight: 500
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    visibility: Window.Windowed
    title: "Activity Monitor"

    property int activeTab: 0 // 0: Processes, 1: Performance, 2: Services
    property int contextPid: -1
    property string contextName: ""
    property bool contextProtected: false

    // Responsive proportional column widths
    readonly property int colWidthPid: 66
    readonly property int colWidthUser: 84
    readonly property int colWidthCpu: 74
    readonly property int colWidthMem: 86
    readonly property int colWidthDisk: 88
    readonly property int colWidthState: 86
    readonly property int colSpacing: 8
    readonly property int colWidthName: Math.max(180, procList.width - 24 - (colWidthPid + colWidthUser + colWidthCpu + colWidthMem + colWidthDisk + colWidthState + colSpacing * 6))

    onActiveTabChanged: {
        if (typeof monitorBridge !== "undefined") {
            monitorBridge.activeTab = activeTab
        }
        if (activeTab === 1) {
            Qt.callLater(repaintAllCharts)
        }
    }

    onVisibleChanged: {
        if (typeof monitorBridge !== "undefined") {
            monitorBridge.isVisible = visible
        }
    }

    function repaintAllCharts() {
        if (typeof cpuCanvas !== "undefined") cpuCanvas.requestPaint()
        if (typeof memCanvas !== "undefined") memCanvas.requestPaint()
        if (typeof gpuCanvas !== "undefined") gpuCanvas.requestPaint()
        if (typeof ioCanvas !== "undefined") ioCanvas.requestPaint()
    }

    function drawSparkline(ctx, w, h, series, maxVal, strokeColor, fillTop, fillBot, drawGrid) {
        if (drawGrid) {
            ctx.strokeStyle = "rgba(255, 255, 255, 0.06)"
            ctx.lineWidth = 1
            for (var g = 1; g <= 3; ++g) {
                var gy = Math.round((h / 4.0) * g) + 0.5
                ctx.beginPath()
                ctx.moveTo(0, gy)
                ctx.lineTo(w, gy)
                ctx.stroke()
            }
        }
        if (!series || series.length < 2 || maxVal <= 0) return

        var step = w / 59.0
        ctx.beginPath()
        ctx.moveTo((60 - series.length) * step, h)
        for (var i = 0; i < series.length; ++i) {
            var val = Math.min(maxVal, Math.max(0.0, series[i]))
            var x = (60 - series.length + i) * step
            var y = h - (val / maxVal) * (h - 4) - 2
            ctx.lineTo(x, y)
        }
        ctx.lineTo(w, h)
        ctx.closePath()

        var grad = ctx.createLinearGradient(0, 0, 0, h)
        grad.addColorStop(0, fillTop)
        grad.addColorStop(1, fillBot)
        ctx.fillStyle = grad
        ctx.fill()

        ctx.beginPath()
        for (var j = 0; j < series.length; ++j) {
            var v = Math.min(maxVal, Math.max(0.0, series[j]))
            var px = (60 - series.length + j) * step
            var py = h - (v / maxVal) * (h - 4) - 2
            if (j === 0) ctx.moveTo(px, py)
            else ctx.lineTo(px, py)
        }
        ctx.strokeStyle = strokeColor
        ctx.lineWidth = 2
        ctx.stroke()
    }

    Timer {
        interval: 1500
        running: rootWindow.visible && rootWindow.visibility !== Window.Minimized
        repeat: true
        onTriggered: monitorBridge.refresh()
    }

    Rectangle {
        id: windowSurface
        anchors.fill: parent
        radius: 12
        color: Qt.rgba(16 / 255, 18 / 255, 25 / 255, 0.88)
        border.color: Qt.rgba(1, 1, 1, 0.12)
        border.width: 1
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // ── Top Titlebar & Segmented Tabs ───────────────────────
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                z: 10

                LiquidGlass {
                    anchors.fill: parent
                    materialType: "toolbar"
                    cornerRadius: 0
                    fluidOpacity: 0.72
                    shadowEnabled: false
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.08)
                }

                MouseArea {
                    anchors.fill: parent
                    onPressed: rootWindow.startSystemMove()
                }

                // Left: Traffic lights & App Identity
                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12

                    MacTrafficLights {
                        targetWindow: rootWindow
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Row {
                        spacing: 7
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            width: 16
                            height: 16
                            source: "image://tinexus/utilities-system-monitor"
                            sourceSize: Qt.size(32, 32)
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            anchors.verticalCenter: parent.verticalCenter
                            visible: status === Image.Ready
                        }

                        Text {
                            text: "Activity Monitor"
                            color: "#F3F4F6"
                            font.family: "Inter"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // Center: Segmented Tab Bar
                Rectangle {
                    anchors.centerIn: parent
                    height: 30
                    width: 312
                    radius: 8
                    color: Qt.rgba(0, 0, 0, 0.35)
                    border.color: Qt.rgba(1, 1, 1, 0.09)
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
                                radius: 6
                                color: rootWindow.activeTab === modelData.tab
                                       ? Qt.rgba(1, 1, 1, 0.16)
                                       : (tabMa.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")
                                Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    color: rootWindow.activeTab === modelData.tab ? "#FFFFFF" : "#9CA3AF"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    font.weight: rootWindow.activeTab === modelData.tab ? Font.Medium : Font.Normal
                                }

                                MouseArea {
                                    id: tabMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: rootWindow.activeTab = modelData.tab
                                }
                            }
                        }
                    }
                }

                // Right: Live CPU/Temp Quick Pill & Refresh
                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    Rectangle {
                        height: 24
                        width: liveMetaRow.implicitWidth + 16
                        radius: 12
                        color: Qt.rgba(0, 0, 0, 0.28)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Row {
                            id: liveMetaRow
                            anchors.centerIn: parent
                            spacing: 6

                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                color: "#30D158"
                                anchors.verticalCenter: parent.verticalCenter

                                SequentialAnimation on opacity {
                                    running: rootWindow.visible && rootWindow.visibility !== Window.Minimized
                                    loops: Animation.Infinite
                                    PropertyAnimation { from: 0.45; to: 1.0; duration: 750; easing.type: Easing.InOutQuad }
                                    PropertyAnimation { from: 1.0; to: 0.45; duration: 750; easing.type: Easing.InOutQuad }
                                }
                            }

                            Text {
                                text: "CPU " + monitorBridge.cpuUsage.toFixed(0) + "%  ·  " + monitorBridge.cpuTemperatureStr
                                color: "#D1D5DB"
                                font.family: "JetBrains Mono"
                                font.pixelSize: 10
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }

                    Rectangle {
                        width: 26
                        height: 26
                        radius: 6
                        color: refreshMa.pressed ? Qt.rgba(1, 1, 1, 0.16) : (refreshMa.containsMouse ? Qt.rgba(1, 1, 1, 0.10) : "transparent")
                        Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
                        anchors.verticalCenter: parent.verticalCenter

                        Image {
                            id: refreshIcon
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: "image://tinexus/view-refresh-symbolic"
                            sourceSize: Qt.size(28, 28)
                            fillMode: Image.PreserveAspectFit
                            opacity: 0.88
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

            // ── Tab 0: Processes ────────────────────────────────────
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
                            Layout.preferredWidth: 250
                            Layout.preferredHeight: 30
                            radius: 8
                            color: Qt.rgba(0, 0, 0, 0.34)
                            border.color: searchInput.activeFocus ? "#0A84FF" : Qt.rgba(1, 1, 1, 0.10)
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 9
                                anchors.rightMargin: 8
                                spacing: 6

                                Image {
                                    width: 13
                                    height: 13
                                    source: "image://tinexus/edit-find-symbolic"
                                    sourceSize: Qt.size(26, 26)
                                    opacity: 0.65
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                TextInput {
                                    id: searchInput
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: "#F3F4F6"
                                    font.family: "Inter"
                                    font.pixelSize: 12
                                    verticalAlignment: TextInput.AlignVCenter
                                    clip: true

                                    Text {
                                        anchors.fill: parent
                                        text: "Filter by name, command, or PID..."
                                        color: "#6B7280"
                                        font.family: "Inter"
                                        font.pixelSize: 12
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                        visible: !searchInput.text
                                    }

                                    onTextChanged: monitorBridge.searchQuery = text
                                }

                                Rectangle {
                                    width: 18
                                    height: 18
                                    radius: 9
                                    color: clearSearchMa.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : "transparent"
                                    visible: searchInput.text.length > 0

                                    Image {
                                        anchors.centerIn: parent
                                        width: 11
                                        height: 11
                                        source: "image://tinexus/window-close-symbolic"
                                        sourceSize: Qt.size(22, 22)
                                        opacity: 0.75
                                    }

                                    MouseArea {
                                        id: clearSearchMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: searchInput.text = ""
                                    }
                                }
                            }
                        }

                        // Category Filter Pills
                        Rectangle {
                            Layout.preferredWidth: 234
                            Layout.preferredHeight: 30
                            radius: 8
                            color: Qt.rgba(0, 0, 0, 0.34)
                            border.color: Qt.rgba(1, 1, 1, 0.10)
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
                                        radius: 6
                                        color: monitorBridge.category === modelData.cat ? Qt.rgba(1, 1, 1, 0.16) : "transparent"

                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.label
                                            color: monitorBridge.category === modelData.cat ? "#FFFFFF" : "#9CA3AF"
                                            font.family: "Inter"
                                            font.pixelSize: 11
                                            font.weight: monitorBridge.category === modelData.cat ? Font.Medium : Font.Normal
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

                        Text {
                            text: monitorBridge.processCount + " Processes"
                            color: "#9CA3AF"
                            font.family: "Inter"
                            font.pixelSize: 11
                            Layout.alignment: Qt.AlignVCenter
                        }

                        // Inspect Button
                        Rectangle {
                            Layout.preferredWidth: 74
                            Layout.preferredHeight: 28
                            radius: 7
                            color: monitorBridge.selectedPid > 0 ? (inspectBtnMa.containsMouse ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.08)) : Qt.rgba(1, 1, 1, 0.03)
                            border.color: monitorBridge.selectedPid > 0 ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.05)
                            border.width: 1
                            opacity: monitorBridge.selectedPid > 0 ? 1.0 : 0.4

                            Text {
                                anchors.centerIn: parent
                                text: "Inspect"
                                color: "#E5E7EB"
                                font.family: "Inter"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                            }

                            MouseArea {
                                id: inspectBtnMa
                                anchors.fill: parent
                                enabled: monitorBridge.selectedPid > 0
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: monitorBridge.inspectProcess(monitorBridge.selectedPid)
                            }
                        }

                        // Quit Process Button
                        Rectangle {
                            Layout.preferredWidth: 88
                            Layout.preferredHeight: 28
                            radius: 7
                            color: monitorBridge.selectedPid > 0 ? (endMa.containsMouse ? "#2A2E3D" : "#1F2330") : "#161822"
                            border.color: monitorBridge.selectedPid > 0 ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.05)
                            border.width: 1
                            opacity: monitorBridge.selectedPid > 0 ? 1.0 : 0.4

                            Text {
                                anchors.centerIn: parent
                                text: "Quit Process"
                                color: "#E5E7EB"
                                font.family: "Inter"
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
                            Layout.preferredWidth: 84
                            Layout.preferredHeight: 28
                            radius: 7
                            color: monitorBridge.selectedPid > 0 ? (forceMa.containsMouse ? "#991B1B" : "#7F1D1D") : "#161822"
                            border.color: monitorBridge.selectedPid > 0 ? "#EF4444" : Qt.rgba(1, 1, 1, 0.05)
                            border.width: 1
                            opacity: monitorBridge.selectedPid > 0 ? 1.0 : 0.4

                            Text {
                                anchors.centerIn: parent
                                text: "Force Quit"
                                color: "#FCA5A5"
                                font.family: "Inter"
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
                        radius: 10
                        color: Qt.rgba(22 / 255, 25 / 255, 34 / 255, 0.72)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            // Table Header
                            Rectangle {
                                Layout.fillWidth: true
                                height: 32
                                color: Qt.rgba(1, 1, 1, 0.04)

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    height: 1
                                    color: Qt.rgba(1, 1, 1, 0.07)
                                }

                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: rootWindow.colSpacing

                                    Component {
                                        id: headerColComp
                                        Item {
                                            property string title
                                            property string colKey
                                            height: parent ? parent.height : 32

                                            Row {
                                                anchors.fill: parent
                                                spacing: 4

                                                Text {
                                                    text: title
                                                    color: monitorBridge.sortColumn === colKey ? "#38BDF8" : "#9CA3AF"
                                                    font.family: "Inter"
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
                                        onLoaded: { item.title = "Process Name"; item.colKey = "name"; }
                                    }
                                    Loader {
                                        width: rootWindow.colWidthPid; height: parent.height
                                        sourceComponent: headerColComp
                                        onLoaded: { item.title = "PID"; item.colKey = "pid"; }
                                    }
                                    Loader {
                                        width: rootWindow.colWidthUser; height: parent.height
                                        sourceComponent: headerColComp
                                        onLoaded: { item.title = "User"; item.colKey = "user"; }
                                    }
                                    Loader {
                                        width: rootWindow.colWidthCpu; height: parent.height
                                        sourceComponent: headerColComp
                                        onLoaded: { item.title = "% CPU"; item.colKey = "cpu"; }
                                    }
                                    Loader {
                                        width: rootWindow.colWidthMem; height: parent.height
                                        sourceComponent: headerColComp
                                        onLoaded: { item.title = "Memory"; item.colKey = "memory"; }
                                    }
                                    Loader {
                                        width: rootWindow.colWidthDisk; height: parent.height
                                        sourceComponent: headerColComp
                                        onLoaded: { item.title = "Disk I/O"; item.colKey = "disk"; }
                                    }
                                    Loader {
                                        width: rootWindow.colWidthState; height: parent.height
                                        sourceComponent: headerColComp
                                        onLoaded: { item.title = "State"; item.colKey = "state"; }
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
                                        savedContentY = contentY
                                    }
                                }

                                Connections {
                                    target: monitorBridge
                                    function onProcessesChanged() {
                                        if (procList.savedContentY > 0 && !(procList.moving || procList.dragging || procList.flicking)) {
                                            var targetY = procList.savedContentY
                                            Qt.callLater(function() {
                                                if (procList.contentHeight > procList.height) {
                                                    procList.contentY = Math.min(targetY, procList.contentHeight - procList.height)
                                                }
                                            })
                                        }
                                    }
                                }

                                delegate: Rectangle {
                                    id: procRow
                                    width: procList.width
                                    height: 30
                                    color: {
                                        if (modelData.pid === monitorBridge.selectedPid) return "#0A84FF"
                                        if (rowMa.containsMouse) return Qt.rgba(1, 1, 1, 0.06)
                                        return index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.018)
                                    }
                                    Behavior on color { ColorAnimation { duration: 100; easing.type: Easing.OutCubic } }

                                    MouseArea {
                                        id: rowMa
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: function(mouse) {
                                            procContextMenu.visible = false
                                            monitorBridge.selectProcess(modelData.pid)
                                            if (mouse.button === Qt.RightButton) {
                                                rootWindow.contextPid = modelData.pid
                                                rootWindow.contextName = modelData.name
                                                rootWindow.contextProtected = modelData.isProtected
                                                var pt = rowMa.mapToItem(processesView, mouse.x, mouse.y)
                                                procContextMenu.x = Math.min(pt.x, processesView.width - procContextMenu.width - 10)
                                                procContextMenu.y = Math.min(pt.y, processesView.height - procContextMenu.height - 10)
                                                procContextMenu.visible = true
                                            }
                                        }
                                        onDoubleClicked: function(mouse) {
                                            if (mouse.button === Qt.LeftButton) {
                                                monitorBridge.inspectProcess(modelData.pid)
                                            }
                                        }
                                    }

                                    Row {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: rootWindow.colSpacing

                                        Item {
                                            width: rootWindow.colWidthName
                                            height: parent.height
                                            clip: true

                                            Row {
                                                anchors.fill: parent
                                                spacing: 8

                                                Item {
                                                    width: 16
                                                    height: 16
                                                    anchors.verticalCenter: parent.verticalCenter

                                                    Image {
                                                        id: procIcon
                                                        anchors.fill: parent
                                                        sourceSize.width: 32
                                                        sourceSize.height: 32
                                                        fillMode: Image.PreserveAspectFit
                                                        asynchronous: true
                                                        smooth: true
                                                        source: {
                                                            var nm = modelData.iconName || modelData.name
                                                            return nm ? ("image://tinexus/" + nm.toLowerCase()) : "image://tinexus/application-x-executable"
                                                        }
                                                        visible: status === Image.Ready
                                                    }

                                                    Image {
                                                        anchors.fill: parent
                                                        sourceSize.width: 32
                                                        sourceSize.height: 32
                                                        fillMode: Image.PreserveAspectFit
                                                        visible: !procIcon.visible
                                                        source: modelData.isProtected
                                                                ? "image://tinexus/security-high-symbolic"
                                                                : (modelData.isSystem
                                                                   ? "image://tinexus/preferences-system-services"
                                                                   : "image://tinexus/application-x-executable")
                                                        opacity: 0.85
                                                    }
                                                }

                                                Text {
                                                    width: parent.width - 26
                                                    text: modelData.name
                                                    color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#F3F4F6"
                                                    font.family: "Inter"
                                                    font.pixelSize: 12
                                                    font.weight: modelData.pid === monitorBridge.selectedPid ? Font.Medium : Font.Normal
                                                    elide: Text.ElideRight
                                                    anchors.verticalCenter: parent.verticalCenter
                                                }
                                            }
                                        }

                                        Text {
                                            width: rootWindow.colWidthPid
                                            text: modelData.pid
                                            color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#9CA3AF"
                                            font.family: "JetBrains Mono"
                                            font.pixelSize: 11
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            width: rootWindow.colWidthUser
                                            text: modelData.user
                                            color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#9CA3AF"
                                            font.family: "Inter"
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            width: rootWindow.colWidthCpu
                                            text: modelData.cpuStr
                                            color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : (modelData.cpu > 10.0 ? "#F87171" : "#E5E7EB")
                                            font.family: "JetBrains Mono"
                                            font.pixelSize: 11
                                            font.weight: modelData.cpu > 5.0 ? Font.Medium : Font.Normal
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            width: rootWindow.colWidthMem
                                            text: modelData.rssStr
                                            color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#E5E7EB"
                                            font.family: "JetBrains Mono"
                                            font.pixelSize: 11
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            width: rootWindow.colWidthDisk
                                            text: modelData.diskStr
                                            color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#9CA3AF"
                                            font.family: "JetBrains Mono"
                                            font.pixelSize: 11
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Text {
                                            width: rootWindow.colWidthState
                                            text: modelData.state
                                            color: modelData.pid === monitorBridge.selectedPid ? "#FFFFFF" : "#9CA3AF"
                                            font.family: "Inter"
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

                // Right-click backdrop dismiss
                MouseArea {
                    anchors.fill: parent
                    visible: procContextMenu.visible
                    z: 20
                    onClicked: procContextMenu.visible = false
                }

                // Floating Context Menu
                Rectangle {
                    id: procContextMenu
                    visible: false
                    width: 196
                    height: menuCol.implicitHeight + 12
                    radius: 9
                    color: Qt.rgba(24 / 255, 27 / 255, 38 / 255, 0.96)
                    border.color: Qt.rgba(1, 1, 1, 0.14)
                    border.width: 1
                    z: 30

                    Column {
                        id: menuCol
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 2

                        Rectangle {
                            width: parent.width
                            height: 28
                            radius: 6
                            color: ctxInspectMa.containsMouse ? "#0A84FF" : "transparent"
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                text: "Inspect Process..."
                                color: "#F3F4F6"
                                font.family: "Inter"
                                font.pixelSize: 12
                            }
                            MouseArea {
                                id: ctxInspectMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    procContextMenu.visible = false
                                    monitorBridge.inspectProcess(rootWindow.contextPid)
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 28
                            radius: 6
                            color: (!rootWindow.contextProtected && ctxQuitMa.containsMouse) ? "#0A84FF" : "transparent"
                            opacity: rootWindow.contextProtected ? 0.45 : 1.0
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                text: "Quit Process (SIGTERM)"
                                color: "#F3F4F6"
                                font.family: "Inter"
                                font.pixelSize: 12
                            }
                            MouseArea {
                                id: ctxQuitMa
                                anchors.fill: parent
                                enabled: !rootWindow.contextProtected
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    procContextMenu.visible = false
                                    monitorBridge.selectProcess(rootWindow.contextPid)
                                    monitorBridge.openKillDialog(false)
                                }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 28
                            radius: 6
                            color: (!rootWindow.contextProtected && ctxKillMa.containsMouse) ? "#B91C1C" : "transparent"
                            opacity: rootWindow.contextProtected ? 0.45 : 1.0
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                text: "Force Quit (SIGKILL)"
                                color: rootWindow.contextProtected ? "#9CA3AF" : "#FCA5A5"
                                font.family: "Inter"
                                font.pixelSize: 12
                            }
                            MouseArea {
                                id: ctxKillMa
                                anchors.fill: parent
                                enabled: !rootWindow.contextProtected
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    procContextMenu.visible = false
                                    monitorBridge.selectProcess(rootWindow.contextPid)
                                    monitorBridge.openKillDialog(true)
                                }
                            }
                        }
                    }
                }
            }

            // ── Tab 1: Performance Telemetry ────────────────────────
            Item {
                id: performanceView
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: rootWindow.activeTab === 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    // Top Hardware Telemetry Strip (Genuine sysfs/procfs readouts)
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 42
                        radius: 9
                        color: Qt.rgba(22 / 255, 25 / 255, 34 / 255, 0.72)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 18

                            Text {
                                text: "TEMP  " + monitorBridge.cpuTemperatureStr
                                color: "#38BDF8"
                                font.family: "JetBrains Mono"
                                font.pixelSize: 11
                            }
                            Text {
                                text: "GPU  " + (monitorBridge.gpuAvailable ? (monitorBridge.gpuBusyPct.toFixed(0) + "%") : monitorBridge.gpuDriver)
                                color: "#34D399"
                                font.family: "JetBrains Mono"
                                font.pixelSize: 11
                            }
                            Text {
                                text: monitorBridge.hasBattery
                                      ? ("PWR  " + monitorBridge.batteryPercent + "% (" + monitorBridge.powerStatus + (monitorBridge.powerWatts > 0 ? (", " + monitorBridge.powerWatts.toFixed(1) + "W") : "") + ")")
                                      : "PWR  AC Supply"
                                color: "#FBBF24"
                                font.family: "JetBrains Mono"
                                font.pixelSize: 11
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: "NIC  " + monitorBridge.netActiveInterface + "   ·   " + monitorBridge.diskIops + " IOPS"
                                color: "#9CA3AF"
                                font.family: "JetBrains Mono"
                                font.pixelSize: 11
                            }
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        columns: 2
                        rowSpacing: 12
                        columnSpacing: 12

                        // 1. CPU & Per-Core Card
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 10
                            color: Qt.rgba(22 / 255, 25 / 255, 34 / 255, 0.72)
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
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: monitorBridge.cpuUsage.toFixed(1) + "%"
                                        color: "#38BDF8"
                                        font.family: "JetBrains Mono"
                                        font.pixelSize: 14
                                        font.weight: Font.Bold
                                    }
                                }

                                Text {
                                    text: monitorBridge.cpuModel + " (" + monitorBridge.cpuCoresCount + " Cores)"
                                    color: "#9CA3AF"
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Canvas {
                                    id: cpuCanvas
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.fillStyle = "#101219"
                                        ctx.fillRect(0, 0, width, height)
                                        rootWindow.drawSparkline(ctx, width, height, monitorBridge.cpuHistory, 100.0,
                                                                 "#38BDF8", "rgba(56, 189, 248, 0.35)", "rgba(56, 189, 248, 0.02)", true)
                                    }
                                    Connections {
                                        target: monitorBridge
                                        function onHistoryChanged() {
                                            if (rootWindow.activeTab === 1) cpuCanvas.requestPaint()
                                        }
                                    }
                                }

                                // Per-Core Live Bars
                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 16
                                    spacing: 4
                                    Repeater {
                                        model: monitorBridge.coreUsages
                                        Rectangle {
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: 6
                                            radius: 3
                                            color: "#1F2433"
                                            Rectangle {
                                                width: parent.width * Math.min(1.0, Math.max(0.0, modelData.usage / 100.0))
                                                height: parent.height
                                                radius: 3
                                                color: modelData.usage > 80 ? "#F87171" : "#38BDF8"
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 2. Real Memory & Swap Card
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 10
                            color: Qt.rgba(22 / 255, 25 / 255, 34 / 255, 0.72)
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: "Physical Memory (RAM)"
                                        color: "#F3F4F6"
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: monitorBridge.memUsedGb + " / " + monitorBridge.memTotalGb + " (" + monitorBridge.memUsagePct.toFixed(0) + "%)"
                                        color: "#C084FC"
                                        font.family: "JetBrains Mono"
                                        font.pixelSize: 12
                                        font.weight: Font.Bold
                                    }
                                }

                                Text {
                                    text: "Cached: " + monitorBridge.memCachedGb + "   ·   Swap: " + monitorBridge.memSwapUsedGb + " / " + monitorBridge.memSwapTotalGb
                                    color: "#9CA3AF"
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 6
                                    radius: 3
                                    color: "#1F2433"
                                    Rectangle {
                                        width: parent.width * Math.min(1.0, Math.max(0.0, monitorBridge.memUsagePct / 100.0))
                                        height: parent.height
                                        radius: 3
                                        color: "#A855F7"
                                    }
                                }

                                Canvas {
                                    id: memCanvas
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.fillStyle = "#101219"
                                        ctx.fillRect(0, 0, width, height)
                                        rootWindow.drawSparkline(ctx, width, height, monitorBridge.memHistory, 100.0,
                                                                 "#A855F7", "rgba(168, 85, 247, 0.35)", "rgba(168, 85, 247, 0.02)", true)
                                    }
                                    Connections {
                                        target: monitorBridge
                                        function onHistoryChanged() {
                                            if (rootWindow.activeTab === 1) memCanvas.requestPaint()
                                        }
                                    }
                                }
                            }
                        }

                        // 3. GPU Utilization Card
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 10
                            color: Qt.rgba(22 / 255, 25 / 255, 34 / 255, 0.72)
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: "Graphics (" + monitorBridge.gpuVendor + ")"
                                        color: "#F3F4F6"
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: monitorBridge.gpuAvailable ? (monitorBridge.gpuBusyPct.toFixed(0) + "%") : monitorBridge.gpuDriver
                                        color: "#34D399"
                                        font.family: "JetBrains Mono"
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                    }
                                }

                                Text {
                                    text: monitorBridge.gpuDeviceName + " — " + monitorBridge.gpuTelemetryStatus
                                    color: "#9CA3AF"
                                    font.family: "Inter"
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Canvas {
                                    id: gpuCanvas
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.fillStyle = "#101219"
                                        ctx.fillRect(0, 0, width, height)
                                        rootWindow.drawSparkline(ctx, width, height, monitorBridge.gpuHistory, 100.0,
                                                                 "#34D399", "rgba(52, 211, 153, 0.32)", "rgba(52, 211, 153, 0.02)", true)
                                    }
                                    Connections {
                                        target: monitorBridge
                                        function onHistoryChanged() {
                                            if (rootWindow.activeTab === 1) gpuCanvas.requestPaint()
                                        }
                                    }
                                }
                            }
                        }

                        // 4. Storage & Network I/O Card
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 10
                            color: Qt.rgba(22 / 255, 25 / 255, 34 / 255, 0.72)
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        text: "Disk & Network I/O"
                                        color: "#F3F4F6"
                                        font.family: "Inter"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                    }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: "↓ " + monitorBridge.netRxRate + "  ↑ " + monitorBridge.netTxRate
                                        color: "#FBBF24"
                                        font.family: "JetBrains Mono"
                                        font.pixelSize: 11
                                        font.weight: Font.Bold
                                    }
                                }

                                Text {
                                    text: "Disk R: " + monitorBridge.diskReadRate + "  W: " + monitorBridge.diskWriteRate + " (" + monitorBridge.diskIops + " IOPS)"
                                    color: "#9CA3AF"
                                    font.family: "JetBrains Mono"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Canvas {
                                    id: ioCanvas
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    onPaint: {
                                        var ctx = getContext("2d")
                                        ctx.reset()
                                        ctx.fillStyle = "#101219"
                                        ctx.fillRect(0, 0, width, height)

                                        var netH = monitorBridge.netHistory
                                        var diskH = monitorBridge.diskHistory
                                        var maxVal = 250.0
                                        for (var i = 0; i < netH.length; ++i) {
                                            if (netH[i] * 1.2 > maxVal) maxVal = netH[i] * 1.2
                                        }
                                        for (var j = 0; j < diskH.length; ++j) {
                                            if (diskH[j] * 1.2 > maxVal) maxVal = diskH[j] * 1.2
                                        }
                                        rootWindow.drawSparkline(ctx, width, height, diskH, maxVal,
                                                                 "#10B981", "rgba(16, 185, 129, 0.28)", "rgba(16, 185, 129, 0.01)", true)
                                        rootWindow.drawSparkline(ctx, width, height, netH, maxVal,
                                                                 "#F59E0B", "rgba(245, 158, 11, 0.25)", "rgba(245, 158, 11, 0.01)", false)
                                    }
                                    Connections {
                                        target: monitorBridge
                                        function onHistoryChanged() {
                                            if (rootWindow.activeTab === 1) ioCanvas.requestPaint()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ── Tab 2: Supervised Services ──────────────────────────
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
                            text: "Supervised Platform Services (tinexus-serviced)"
                            color: "#F3F4F6"
                            font.family: "Inter"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: monitorBridge.services.length + " Daemons"
                            color: "#9CA3AF"
                            font.family: "Inter"
                            font.pixelSize: 12
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 10
                        color: Qt.rgba(22 / 255, 25 / 255, 34 / 255, 0.72)
                        border.color: Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1
                        clip: true

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            Rectangle {
                                Layout.fillWidth: true
                                height: 32
                                color: Qt.rgba(1, 1, 1, 0.04)

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 8

                                    Text { Layout.preferredWidth: 185; text: "Service Name"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11; font.weight: Font.DemiBold }
                                    Text { Layout.fillWidth: true; text: "Role & Architecture Layer"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11; font.weight: Font.DemiBold }
                                    Text { Layout.preferredWidth: 100; text: "Process ID"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11; font.weight: Font.DemiBold }
                                    Text { Layout.preferredWidth: 90; text: "Status"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11; font.weight: Font.DemiBold }
                                    Text { Layout.preferredWidth: 88; text: "Action"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11; font.weight: Font.DemiBold }
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
                                    color: index % 2 === 0 ? "transparent" : Qt.rgba(1, 1, 1, 0.018)

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        spacing: 8

                                        RowLayout {
                                            Layout.preferredWidth: 185
                                            spacing: 8

                                            Image {
                                                width: 15
                                                height: 15
                                                source: modelData.isProtected ? "image://tinexus/security-high-symbolic" : "image://tinexus/preferences-system-services"
                                                sourceSize: Qt.size(30, 30)
                                                fillMode: Image.PreserveAspectFit
                                            }

                                            Text {
                                                text: modelData.name
                                                color: "#F3F4F6"
                                                font.family: "Inter"
                                                font.pixelSize: 12
                                                font.weight: Font.Medium
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }

                                        Text {
                                            Layout.fillWidth: true
                                            text: modelData.role
                                            color: "#9CA3AF"
                                            font.family: "Inter"
                                            font.pixelSize: 11
                                            elide: Text.ElideRight
                                        }

                                        Text {
                                            Layout.preferredWidth: 100
                                            text: modelData.pidStr
                                            color: modelData.active ? "#38BDF8" : "#6B7280"
                                            font.family: "JetBrains Mono"
                                            font.pixelSize: 11
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 82
                                            Layout.preferredHeight: 20
                                            radius: 10
                                            color: modelData.active ? Qt.rgba(0.06, 0.73, 0.51, 0.15) : Qt.rgba(0.96, 0.62, 0.04, 0.15)
                                            border.color: modelData.active ? "#10B981" : "#F59E0B"
                                            border.width: 1

                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.status
                                                color: modelData.active ? "#10B981" : "#F59E0B"
                                                font.family: "Inter"
                                                font.pixelSize: 10
                                                font.weight: Font.Bold
                                            }
                                        }

                                        Rectangle {
                                            Layout.preferredWidth: 82
                                            Layout.preferredHeight: 24
                                            radius: 6
                                            color: modelData.isProtected ? "#161822" : (rstMa.containsMouse ? "#2A2E3D" : "#1F2330")
                                            border.color: modelData.isProtected ? Qt.rgba(1, 1, 1, 0.05) : Qt.rgba(1, 1, 1, 0.15)
                                            border.width: 1
                                            opacity: modelData.isProtected ? 0.45 : 1.0

                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.isProtected ? "Protected" : "Restart"
                                                color: modelData.isProtected ? "#6B7280" : "#E5E7EB"
                                                font.family: "Inter"
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

        // ── Process Inspect Modal ───────────────────────────────────
        Rectangle {
            id: inspectModal
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.60)
            visible: monitorBridge.inspectModalOpen
            z: 90

            MouseArea {
                anchors.fill: parent
                onClicked: monitorBridge.closeInspectModal()
            }

            Rectangle {
                anchors.centerIn: parent
                width: 490
                height: 360
                radius: 12
                color: "#1B1E2A"
                border.color: Qt.rgba(1, 1, 1, 0.14)
                border.width: 1

                MouseArea { anchors.fill: parent }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 12

                    RowLayout {
                        spacing: 10
                        Image {
                            width: 28
                            height: 28
                            sourceSize: Qt.size(56, 56)
                            fillMode: Image.PreserveAspectFit
                            source: {
                                var d = monitorBridge.inspectedProcess
                                var ic = d.iconName || d.name || "application-x-executable"
                                return "image://tinexus/" + ic.toLowerCase()
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: monitorBridge.inspectedProcess.name || "Process"
                                color: "#FFFFFF"
                                font.family: "Inter"
                                font.pixelSize: 15
                                font.weight: Font.Bold
                            }
                            Text {
                                text: "PID " + (monitorBridge.inspectedProcess.pid || 0) + "  ·  Parent " + (monitorBridge.inspectedProcess.ppid || 0) + "  ·  " + (monitorBridge.inspectedProcess.user || "")
                                color: "#9CA3AF"
                                font.family: "JetBrains Mono"
                                font.pixelSize: 11
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Qt.rgba(1, 1, 1, 0.08)
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 16

                        Text { text: "CPU Usage"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11 }
                        Text { text: monitorBridge.inspectedProcess.cpuStr || "0.0%"; color: "#38BDF8"; font.family: "JetBrains Mono"; font.pixelSize: 11 }

                        Text { text: "Resident Memory"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11 }
                        Text { text: monitorBridge.inspectedProcess.rssStr || "0 KB"; color: "#F3F4F6"; font.family: "JetBrains Mono"; font.pixelSize: 11 }

                        Text { text: "Disk Throughput"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11 }
                        Text { text: monitorBridge.inspectedProcess.diskStr || "—"; color: "#F3F4F6"; font.family: "JetBrains Mono"; font.pixelSize: 11 }

                        Text { text: "Threads / Open FDs"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11 }
                        Text { text: (monitorBridge.inspectedProcess.threads || 1) + " threads  ·  " + (monitorBridge.inspectedProcess.openFds || "0") + " FDs"; color: "#F3F4F6"; font.family: "JetBrains Mono"; font.pixelSize: 11 }

                        Text { text: "State / Energy Impact"; color: "#9CA3AF"; font.family: "Inter"; font.pixelSize: 11 }
                        Text { text: (monitorBridge.inspectedProcess.state || "Sleeping") + "  ·  " + (monitorBridge.inspectedProcess.energyImpact || "Low"); color: "#F3F4F6"; font.family: "Inter"; font.pixelSize: 11 }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 7
                        color: "#11131B"
                        border.color: Qt.rgba(1, 1, 1, 0.07)
                        border.width: 1
                        clip: true

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 8
                            Text {
                                width: parent.width
                                text: monitorBridge.inspectedProcess.cmdline || ""
                                color: "#D1D5DB"
                                font.family: "JetBrains Mono"
                                font.pixelSize: 10
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            Layout.preferredWidth: 92
                            Layout.preferredHeight: 30
                            radius: 7
                            color: closeInspMa.containsMouse ? "#2A2E3D" : "#222634"
                            border.color: Qt.rgba(1, 1, 1, 0.14)
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "Close"
                                color: "#F3F4F6"
                                font.family: "Inter"
                                font.pixelSize: 12
                            }
                            MouseArea {
                                id: closeInspMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: monitorBridge.closeInspectModal()
                            }
                        }
                    }
                }
            }
        }

        // ── Safety Confirmation Modal Sheet ─────────────────────────
        Rectangle {
            id: killModal
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.65)
            visible: monitorBridge.killModalOpen
            z: 100

            MouseArea { anchors.fill: parent }

            Rectangle {
                anchors.centerIn: parent
                width: 440
                height: monitorBridge.selectedIsProtected ? 220 : 186
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
                        Image {
                            width: 28
                            height: 28
                            sourceSize: Qt.size(56, 56)
                            fillMode: Image.PreserveAspectFit
                            source: monitorBridge.selectedIsProtected
                                    ? "image://tinexus/security-high-symbolic"
                                    : "image://tinexus/dialog-warning-symbolic"
                            Layout.alignment: Qt.AlignTop
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: monitorBridge.killModalForce ? "Force Quit Process?" : "Quit Process?"
                                color: "#F3F4F6"
                                font.family: "Inter"
                                font.pixelSize: 15
                                font.weight: Font.Bold
                            }

                            Text {
                                text: "Do you want to " + (monitorBridge.killModalForce ? "force quit" : "quit") + " \"" + monitorBridge.selectedProcessName + "\" (PID " + monitorBridge.selectedPid + ")?"
                                color: "#D1D5DB"
                                font.family: "Inter"
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                    }

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
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 8
                            Image {
                                width: 15
                                height: 15
                                source: "image://tinexus/dialog-warning-symbolic"
                                sourceSize: Qt.size(30, 30)
                            }
                            Text {
                                text: "Protected system process. Action blocked for stability."
                                color: "#FCA5A5"
                                font.family: "Inter"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            Layout.preferredWidth: 82
                            Layout.preferredHeight: 30
                            radius: 7
                            color: cancelMa.containsMouse ? "#2A2E3D" : "#222634"
                            border.color: Qt.rgba(1, 1, 1, 0.15)
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "Cancel"
                                color: "#E5E7EB"
                                font.family: "Inter"
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
                            Layout.preferredWidth: monitorBridge.killModalForce ? 102 : 92
                            Layout.preferredHeight: 30
                            radius: 7
                            color: monitorBridge.selectedIsProtected ? "#374151" : (actionMa.containsMouse ? "#B91C1C" : "#DC2626")
                            opacity: monitorBridge.selectedIsProtected ? 0.45 : 1.0

                            Text {
                                anchors.centerIn: parent
                                text: monitorBridge.selectedIsProtected ? "Blocked" : (monitorBridge.killModalForce ? "Force Quit" : "Quit")
                                color: "#FFFFFF"
                                font.family: "Inter"
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
}
