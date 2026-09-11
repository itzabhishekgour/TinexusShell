// ============================================================================
// AboutWindow.qml — Qt6 / QML About Tinexus System Profiler
// 5 Tab Modes: Overview, Displays, Storage, Support, Service
// Authentic macOS / Tinexus Liquid Glass proportions
// ============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "../../common/qml"

Window {
    id: rootWindow
    width: 720
    height: 480
    minimumWidth: 680
    minimumHeight: 460
    color: "#161822"
    flags: Qt.Window | Qt.FramelessWindowHint
    visibility: Window.Windowed
    title: "About Tinexus"

    property int activeTab: 0 // 0: Overview, 1: Displays, 2: Storage, 3: Support, 4: Service

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Top Window Bar & Segmented Tabs ─────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: "#1C1F2B"
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
            z: 10

            // System move drag handler
            MouseArea {
                anchors.fill: parent
                onPressed: rootWindow.startSystemMove()
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 12

                MacTrafficLights {
                    targetWindow: rootWindow
                    Layout.alignment: Qt.AlignVCenter
                }

                Item { Layout.fillWidth: true }

                // Top Segmented Tab Switcher
                Rectangle {
                    height: 30
                    width: 440
                    radius: 7
                    color: Qt.rgba(0, 0, 0, 0.35)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Row {
                        anchors.fill: parent
                        anchors.margins: 2
                        spacing: 2

                        readonly property var tabNames: ["Overview", "Displays", "Storage", "Support", "Service"]

                        Repeater {
                            model: parent.tabNames

                            Rectangle {
                                width: (440 - 4 - 8) / 5
                                height: 26
                                radius: 5
                                color: rootWindow.activeTab === index ? Qt.rgba(1, 1, 1, 0.15) : (tabMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.06) : "transparent")

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: rootWindow.activeTab === index ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.60)
                                    font.pixelSize: 12
                                    font.bold: rootWindow.activeTab === index
                                }

                                MouseArea {
                                    id: tabMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: rootWindow.activeTab = index
                                }
                            }
                        }
                    }
                }

                Item { Layout.fillWidth: true }
                Item { width: 40 } // Balance traffic lights
            }
        }

        // ── Main Content Area ───────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            RowLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 20

                // ── Left Column: Emblem & Branding ──────────────────
                ColumnLayout {
                    Layout.preferredWidth: 200
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
                    spacing: 8

                    Item { height: 16 }

                    // Logo Emblem with Ambient Glow
                    Item {
                        Layout.preferredWidth: 104
                        Layout.preferredHeight: 104
                        Layout.alignment: Qt.AlignHCenter

                        // Ambient Glow
                        Rectangle {
                            anchors.centerIn: parent
                            width: 96
                            height: 96
                            radius: 48
                            color: Qt.rgba(0.22, 0.74, 0.97, 0.25)
                        }

                        Rectangle {
                            anchors.centerIn: parent
                            width: 88
                            height: 88
                            radius: 44
                            color: "#1E2230"
                            border.color: "#38BDF8"
                            border.width: 2

                            Image {
                                anchors.centerIn: parent
                                width: 56
                                height: 56
                                source: "file:///usr/share/tinexus/tinexus-logo.png"
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                mipmap: true

                                // Fallback if image not staged yet
                                Text {
                                    anchors.centerIn: parent
                                    text: "✦"
                                    color: "#38BDF8"
                                    font.pixelSize: 32
                                    visible: parent.status !== Image.Ready
                                }
                            }
                        }
                    }

                    Item { height: 8 }

                    Text {
                        text: "Tinexus OS"
                        color: "#F2F2F8"
                        font.pixelSize: 18
                        font.bold: true
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Wayland Desktop Platform"
                        color: "#38BDF8"
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Version 1.0 (LTS)"
                        color: "#707686"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Item { Layout.fillHeight: true }
                }

                // ── Vertical Separator ──────────────────────────────
                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: Qt.rgba(1, 1, 1, 0.08)
                }

                // ── Right Column: Tab View Contents ─────────────────
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // 1. OVERVIEW TAB
                    ColumnLayout {
                        anchors.fill: parent
                        visible: rootWindow.activeTab === 0
                        spacing: 12

                        Text {
                            text: typeof bridge !== "undefined" ? bridge.osTitle : "Tinexus Desktop"
                            color: "#F2F2F8"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Text {
                            text: typeof bridge !== "undefined" ? bridge.osVersion : "Version 1.0 (Architecture Freeze - LTS)"
                            color: "#38BDF8"
                            font.pixelSize: 12
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }

                        // Specs Grouped Card
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 196
                            radius: 10
                            color: "#1E222E"
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 0

                                readonly property var specList: [
                                    { label: "Processor",    val: typeof bridge !== "undefined" ? bridge.cpuModel : "Processor" },
                                    { label: "Memory",       val: typeof bridge !== "undefined" ? bridge.memInfo : "RAM" },
                                    { label: "Graphics",     val: typeof bridge !== "undefined" ? bridge.graphicsInfo : "Graphics" },
                                    { label: "Platform",     val: typeof bridge !== "undefined" ? bridge.platformInfo : "Platform" },
                                    { label: "Kernel",       val: typeof bridge !== "undefined" ? bridge.kernelVersion : "Kernel" },
                                    { label: "Root Device",  val: typeof bridge !== "undefined" ? bridge.rootDevice : "Device" }
                                ]

                                Repeater {
                                    model: parent.specList

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 12

                                            Text {
                                                text: modelData.label
                                                color: "#9EA4B4"
                                                font.pixelSize: 12
                                                font.bold: true
                                                Layout.preferredWidth: 100
                                            }

                                            Text {
                                                text: modelData.val
                                                color: "#F2F2F8"
                                                font.pixelSize: 12
                                                Layout.fillWidth: true
                                                elide: Text.ElideRight
                                            }
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 1
                                            color: Qt.rgba(1, 1, 1, 0.05)
                                            visible: index < 5
                                        }
                                    }
                                }
                            }
                        }

                        // Action Buttons
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignRight
                            spacing: 12

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                width: 140
                                height: 32
                                radius: 6
                                color: btnReportMouse.containsMouse ? "#3C4254" : "#282C38"
                                border.color: Qt.rgba(1, 1, 1, 0.12)
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "System Report..."
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    id: btnReportMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: if (typeof bridge !== "undefined") bridge.launchSystemReport()
                                }
                            }

                            Rectangle {
                                width: 150
                                height: 32
                                radius: 6
                                color: btnUpdateMouse.containsMouse ? "#3C4254" : "#282C38"
                                border.color: Qt.rgba(1, 1, 1, 0.12)
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: "Software Update..."
                                    color: "#FFFFFF"
                                    font.pixelSize: 12
                                }

                                MouseArea {
                                    id: btnUpdateMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: if (typeof bridge !== "undefined") bridge.launchSoftwareUpdate()
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // 2. DISPLAYS TAB
                    ColumnLayout {
                        anchors.fill: parent
                        visible: rootWindow.activeTab === 1
                        spacing: 14

                        Text {
                            text: "Displays & Graphics"
                            color: "#F2F2F8"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Text {
                            text: "High DPI Hardware Accelerated Display Engine"
                            color: "#38BDF8"
                            font.pixelSize: 12
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }

                        // Display Banner
                        RowLayout {
                            spacing: 16

                            Rectangle {
                                width: 54
                                height: 38
                                radius: 6
                                color: "#202430"
                                border.color: "#0A84FF"
                                border.width: 1.5

                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 44
                                    height: 28
                                    color: "#0E1018"

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 6
                                        height: 6
                                        radius: 3
                                        color: "#38BDF8"
                                    }
                                }
                            }

                            ColumnLayout {
                                spacing: 2

                                Text {
                                    text: typeof bridge !== "undefined" ? bridge.displayName : "Built-in Display"
                                    color: "#F2F2F8"
                                    font.pixelSize: 14
                                    font.bold: true
                                }

                                Text {
                                    text: "Primary Display • Active Hardware VSync"
                                    color: "#22C55E"
                                    font.pixelSize: 12
                                }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }

                        // Specs Table
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            readonly property var dispList: [
                                { label: "Resolution",    val: typeof bridge !== "undefined" ? bridge.displayResolution : "" },
                                { label: "Refresh Rate",  val: typeof bridge !== "undefined" ? bridge.displayRefreshRate : "" },
                                { label: "UI Scaling",    val: typeof bridge !== "undefined" ? bridge.displayScale : "" },
                                { label: "Color Format",  val: typeof bridge !== "undefined" ? bridge.displayColorFormat : "" },
                                { label: "Render Engine", val: typeof bridge !== "undefined" ? bridge.displayRenderer : "" }
                            ]

                            Repeater {
                                model: parent.dispList

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Text {
                                        text: modelData.label
                                        color: "#707686"
                                        font.pixelSize: 12
                                        font.bold: true
                                        Layout.preferredWidth: 110
                                    }

                                    Text {
                                        text: modelData.val
                                        color: "#F2F2F8"
                                        font.pixelSize: 12
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // 3. STORAGE TAB
                    ColumnLayout {
                        anchors.fill: parent
                        visible: rootWindow.activeTab === 2
                        spacing: 14

                        Text {
                            text: "Storage Management"
                            color: "#F2F2F8"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Text {
                            text: typeof bridge !== "undefined" ? 
                                  (Math.round(bridge.storageUsedGb) + " GB used of " + Math.round(bridge.storageTotalGb) + " GB (" + Math.round(bridge.storageUsedPct * 100) + "% allocated)") : ""
                            color: "#38BDF8"
                            font.pixelSize: 12
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }

                        // Visual Storage Progress Bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 22
                            radius: 6
                            color: "#232630"
                            border.color: Qt.rgba(1, 1, 1, 0.08)
                            border.width: 1
                            clip: true

                            Row {
                                anchors.fill: parent

                                Rectangle {
                                    height: parent.height
                                    width: typeof bridge !== "undefined" ? Math.max(16, parent.width * bridge.storageUsedPct * 0.65) : 80
                                    color: "#38BDF8" // System & Core
                                }

                                Rectangle {
                                    height: parent.height
                                    width: typeof bridge !== "undefined" ? Math.max(16, parent.width * bridge.storageUsedPct * 0.35) : 40
                                    color: "#0A84FF" // Apps & User
                                }
                            }
                        }

                        // Legend
                        RowLayout {
                            spacing: 24

                            Row {
                                spacing: 6
                                Rectangle { width: 10; height: 10; radius: 5; color: "#38BDF8"; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "System & Core (Rootfs)"; color: "#F2F2F8"; font.pixelSize: 11 }
                            }

                            Row {
                                spacing: 6
                                Rectangle { width: 10; height: 10; radius: 5; color: "#0A84FF"; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "Apps & User Space"; color: "#F2F2F8"; font.pixelSize: 11 }
                            }

                            Row {
                                spacing: 6
                                Rectangle { width: 10; height: 10; radius: 5; color: "#505564"; anchors.verticalCenter: parent.verticalCenter }
                                Text { text: "Available Free Space"; color: "#9EA4B4"; font.pixelSize: 11 }
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }

                        // Detail Table
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            readonly property var storList: [
                                { label: "Mount Point", val: typeof bridge !== "undefined" ? bridge.storageMount : "/" },
                                { label: "Device",      val: typeof bridge !== "undefined" ? bridge.storageDevice : "" },
                                { label: "Filesystem",  val: typeof bridge !== "undefined" ? bridge.storageFsType : "" },
                                { label: "Free Space",  val: typeof bridge !== "undefined" ? (Math.round(bridge.storageFreeGb) + " GB Available") : "" }
                            ]

                            Repeater {
                                model: parent.storList

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Text {
                                        text: modelData.label
                                        color: "#707686"
                                        font.pixelSize: 12
                                        font.bold: true
                                        Layout.preferredWidth: 110
                                    }

                                    Text {
                                        text: modelData.val
                                        color: "#F2F2F8"
                                        font.pixelSize: 12
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // 4. SUPPORT TAB
                    ColumnLayout {
                        anchors.fill: parent
                        visible: rootWindow.activeTab === 3
                        spacing: 14

                        Text {
                            text: "Support & Resources"
                            color: "#F2F2F8"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Text {
                            text: "Engineering Documentation & Platform Guides"
                            color: "#38BDF8"
                            font.pixelSize: 12
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            readonly property var supportList: [
                                { title: "Platform Architecture Spec", sub: "docs/03_SYSTEM_ARCHITECTURE.md" },
                                { title: "Wayland Protocols & Layer Shell", sub: "docs/14_WAYLAND_PROTOCOLS.md" },
                                { title: "Performance Guidelines & Budgets", sub: "docs/08_PERFORMANCE.md" },
                                { title: "Source Code Repository", sub: "github.com/itzabhishekgour/TinexusShell" },
                                { title: "Open-Source Licenses", sub: "GNU GPL v2.0 (Core) & Apache 2.0 (SDK)" }
                            ]

                            Repeater {
                                model: parent.supportList

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: "#0A84FF"
                                        Layout.alignment: Qt.AlignVCenter
                                    }

                                    ColumnLayout {
                                        spacing: 2
                                        Text { text: modelData.title; color: "#F2F2F8"; font.pixelSize: 13; font.bold: true }
                                        Text { text: modelData.sub; color: "#9EA4B4"; font.pixelSize: 11 }
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // 5. SERVICE TAB
                    ColumnLayout {
                        anchors.fill: parent
                        visible: rootWindow.activeTab === 4
                        spacing: 14

                        Text {
                            text: "Platform Supervision"
                            color: "#F2F2F8"
                            font.pixelSize: 18
                            font.bold: true
                        }

                        Text {
                            text: "Supervision Tree & Microservices Status"
                            color: "#38BDF8"
                            font.pixelSize: 12
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.08)
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            ColumnLayout {
                                width: parent.width
                                spacing: 10

                                Repeater {
                                    model: typeof bridge !== "undefined" ? bridge.services : []

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12

                                        Rectangle {
                                            width: 8
                                            height: 8
                                            radius: 4
                                            color: modelData.active ? "#22C55E" : "#38BDF8"
                                            Layout.alignment: Qt.AlignVCenter
                                        }

                                        ColumnLayout {
                                            Layout.preferredWidth: 160
                                            spacing: 1
                                            Text { text: modelData.name; color: "#F2F2F8"; font.pixelSize: 13; font.bold: true }
                                            Text { text: modelData.role; color: "#707686"; font.pixelSize: 11 }
                                        }

                                        Text {
                                            text: modelData.pid
                                            color: "#9EA4B4"
                                            font.pixelSize: 12
                                            Layout.preferredWidth: 80
                                        }

                                        Text {
                                            text: modelData.status
                                            color: modelData.active ? "#22C55E" : "#38BDF8"
                                            font.pixelSize: 11
                                            font.bold: true
                                            Layout.alignment: Qt.AlignRight
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── Bottom Footer ───────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 28
            color: "#13151D"

            Text {
                anchors.centerIn: parent
                text: "Tinexus Platform • Built with Pure C++20 & Wayland"
                color: "#707686"
                font.pixelSize: 11
            }
        }
    }
}
