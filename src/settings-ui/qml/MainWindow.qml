import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    visibility: Window.Windowed
    width: 880
    height: 560
    minimumWidth: 740
    minimumHeight: 480
    title: "Tinexus Settings"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    // ── Navigation History Tracking ────────────────────────────────────────
    property var navHistory: [3]
    property int historyIndex: 0
    property bool internalNav: false
    property real pageTransitionOpacity: 1.0
    property real pageTransitionOffsetX: 0.0

    function getPageTitle(pageIndex) {
        switch (pageIndex) {
            case 0: return "Displays"
            case 1: return "Sound"
            case 2: return "Appearance"
            case 3: return "Wi-Fi"
            case 4: return "Battery & Power"
            case 5: return "Keyboard Shortcuts"
            case 6: return "Privacy & Security"
            case 7: return "About Tinexus"
            case 8: return "Bluetooth"
            case 9: return "Users & Accounts"
            default: return "Settings"
        }
    }

    function openSystemReport() {
        bridge.refreshSystemReport()
        reportModal.visible = true
    }

    ParallelAnimation {
        id: pageEnterAnim
        NumberAnimation {
            target: window
            property: "pageTransitionOpacity"
            from: 0.0
            to: 1.0
            duration: 140
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: window
            property: "pageTransitionOffsetX"
            from: 10.0
            to: 0.0
            duration: 140
            easing.type: Easing.OutCubic
        }
    }

    Connections {
        target: bridge
        function onCurrentPageChanged() {
            toastBanner.opacity = 0.0
            pageEnterAnim.restart()
            if (internalNav) return
            var p = bridge.currentPage
            if (navHistory.length > 0 && navHistory[historyIndex] === p) return
            var newHist = navHistory.slice(0, historyIndex + 1)
            newHist.push(p)
            navHistory = newHist
            historyIndex = navHistory.length - 1
        }
    }

    function goBack() {
        if (historyIndex > 0) {
            historyIndex--
            internalNav = true
            bridge.selectPage(navHistory[historyIndex])
            internalNav = false
        }
    }

    function goForward() {
        if (historyIndex < navHistory.length - 1) {
            historyIndex++
            internalNav = true
            bridge.selectPage(navHistory[historyIndex])
            internalNav = false
        }
    }

    // ── Main Rounded Glass Window Frame ────────────────────────────────────
    Rectangle {
        anchors.fill: parent
        radius: 12
        color: "transparent"
        border.color: Qt.rgba(1, 1, 1, 0.14)
        border.width: 1
        clip: true

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // Left Frosted LiquidGlass Sidebar
            Sidebar {
                id: sidebar
                Layout.fillHeight: true
                Layout.preferredWidth: 240
            }

            // Right Content Area
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "#1b1c22"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // ── Content Top Header Bar ─────────────────────────────
                    Rectangle {
                        Layout.fillWidth: true
                        height: 48
                        color: Qt.rgba(0.11, 0.12, 0.15, 0.90)

                        MouseArea {
                            anchors.fill: parent
                            onPressed: window.startSystemMove()
                            onDoubleClicked: {
                                if (window.visibility === Window.Maximized) {
                                    window.showNormal()
                                } else {
                                    window.showMaximized()
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 20
                            anchors.rightMargin: 20
                            spacing: 12

                            Row {
                                spacing: 4

                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 6
                                    color: backArea.pressed ? "#28ffffff" : (backArea.containsMouse ? "#20ffffff" : "#0dffffff")
                                    Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                    border.color: "#14ffffff"
                                    border.width: 1
                                    opacity: window.historyIndex > 0 ? 1.0 : 0.35

                                    Text {
                                        anchors.centerIn: parent
                                        text: "‹"
                                        color: "#f5f5f7"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: backArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: window.historyIndex > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        onClicked: window.goBack()
                                    }
                                }

                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 6
                                    color: fwdArea.pressed ? "#28ffffff" : (fwdArea.containsMouse ? "#20ffffff" : "#0dffffff")
                                    Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }
                                    border.color: "#14ffffff"
                                    border.width: 1
                                    opacity: window.historyIndex < window.navHistory.length - 1 ? 1.0 : 0.35

                                    Text {
                                        anchors.centerIn: parent
                                        text: "›"
                                        color: "#f5f5f7"
                                        font.pixelSize: 16
                                        font.bold: true
                                    }

                                    MouseArea {
                                        id: fwdArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: window.historyIndex < window.navHistory.length - 1 ? Qt.PointingHandCursor : Qt.ArrowCursor
                                        onClicked: window.goForward()
                                    }
                                }
                            }

                            Text {
                                text: window.getPageTitle(bridge.currentPage)
                                color: "#f5f5f7"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: "#14ffffff"
                        }
                    }

                    // ── Smooth Transitioning Page Stack ────────────────────
                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        StackLayout {
                            id: contentStack
                            anchors.fill: parent
                            currentIndex: bridge.currentPage
                            opacity: window.pageTransitionOpacity
                            transform: Translate { x: window.pageTransitionOffsetX }

                            DisplayPage {}
                            SoundPage {}
                            PersonalizationPage {}
                            NetworkPage {
                                onRequestWifiPassword: (ssid) => wifiModal.openModal(ssid)
                            }
                            SystemPage {}
                            ShortcutsPage {}
                            PrivacyPage {}
                            AboutPage {
                                onRequestSystemReport: window.openSystemReport()
                            }
                            BluetoothPage {}
                            UsersPage {}
                        }
                    }
                }
            }
        }
    }

    // ── Wi-Fi Password Modal ───────────────────────────────────────────────
    WifiModal {
        id: wifiModal
        anchors.fill: parent
        z: 99
    }

    // ── System Report Sheet Modal ──────────────────────────────────────────
    Rectangle {
        id: reportModal
        anchors.fill: parent
        visible: false
        color: Qt.rgba(0, 0, 0, 0.62)
        z: 98

        MouseArea { anchors.fill: parent; onClicked: reportModal.visible = false }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 64, 580)
            height: Math.min(parent.height - 64, 420)
            radius: 12
            color: "#22242c"
            border.color: "#26ffffff"
            border.width: 1

            MouseArea { anchors.fill: parent }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "System Diagnostic Report"
                        color: "#f5f5f7"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        Layout.fillWidth: true
                    }
                    MacButton {
                        text: "Copy Report"
                        primary: true
                        onClicked: bridge.copySystemReport()
                    }
                    MacButton {
                        text: "Close"
                        onClicked: reportModal.visible = false
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 8
                    color: "#15161c"
                    border.color: "#16ffffff"
                    border.width: 1

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 10
                        TextArea {
                            text: bridge.systemReportText
                            readOnly: true
                            color: "#d1d5db"
                            font.family: "JetBrains Mono"
                            font.pixelSize: 11
                            background: null
                        }
                    }
                }
            }
        }
    }

    // ── Toast Notification Banner ──────────────────────────────────────────
    Rectangle {
        id: toastBanner
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 24
        width: Math.min(420, toastText.implicitWidth + 36)
        height: 40
        radius: 8
        color: toastBanner.isError ? "#7f1d1d" : "#242428"
        border.color: toastBanner.isError ? "#ef4444" : "#24ffffff"
        border.width: 1
        opacity: 0.0
        z: 100

        property bool isError: false

        Behavior on opacity {
            NumberAnimation { duration: 180 }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 10

            Text {
                id: toastText
                color: "#f8fafc"
                font.pixelSize: 12
                font.weight: Font.Medium
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }

        Timer {
            id: toastTimer
            interval: 3000
            onTriggered: toastBanner.opacity = 0.0
        }
    }

    Connections {
        target: bridge
        function onToastNotification(message, isError) {
            toastText.text = message
            toastBanner.isError = isError
            toastBanner.opacity = 1.0
            toastTimer.restart()
        }
        function onRequestWifiModal(ssid) {
            wifiModal.openModal(ssid)
        }
        function onDismissWifiModal() {
            wifiModal.closeModal()
        }
    }
}
