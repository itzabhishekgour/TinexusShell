import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window
    visible: true
    visibility: Window.Windowed
    width: 840
    height: 520
    minimumWidth: 720
    minimumHeight: 460
    title: "Tinexus Settings"
    color: "#161618"
    flags: Qt.Window | Qt.FramelessWindowHint

    // ── Navigation History Tracking ────────────────────────────────────────
    property var navHistory: [3] // Default to Wi-Fi/Network
    property int historyIndex: 0
    property bool internalNav: false

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
            default: return "Settings"
        }
    }

    Connections {
        target: bridge
        function onCurrentPageChanged() {
            toastBanner.opacity = 0.0
            if (internalNav) return
            var p = bridge.currentPage
            if (navHistory.length > 0 && navHistory[historyIndex] === p) return
            // Truncate forward history and push new
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

    // ── Main UI Layout ─────────────────────────────────────────────────────
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left Sidebar (with traffic lights and search)
        Sidebar {
            id: sidebar
            Layout.fillHeight: true
            Layout.preferredWidth: 240
        }

        // Right Content Area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e1e" // macOS Dark Mode main canvas (neutral dark graphite, no purple tint)

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // ── macOS Content Top Header Bar (Translucent Frosted Bar) ───
                Rectangle {
                    Layout.fillWidth: true
                    height: 48
                    color: Qt.rgba(0.12, 0.12, 0.12, 0.85) // Frosted glass translucent header

                    // Drag area to move window
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

                        // macOS Back/Forward Chevrons
                        Row {
                            spacing: 4

                            Rectangle {
                                width: 24
                                height: 24
                                radius: 5
                                color: backArea.containsMouse ? "#20ffffff" : "#0dffffff"
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
                                radius: 5
                                color: fwdArea.containsMouse ? "#20ffffff" : "#0dffffff"
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

                        // Page Title (bold center-left next to arrows)
                        Text {
                            text: window.getPageTitle(bridge.currentPage)
                            color: "#f5f5f7"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // Hairline bottom divider
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: "#14ffffff"
                    }
                }

                // ── Stack of Pages ─────────────────────────────────────────
                StackLayout {
                    id: contentStack
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: bridge.currentPage

                    DisplayPage {}
                    SoundPage {}
                    PersonalizationPage {}
                    NetworkPage {
                        onRequestWifiPassword: (ssid) => wifiModal.openModal(ssid)
                    }
                    SystemPage {}
                    ShortcutsPage {}
                    PrivacyPage {}
                    AboutPage {}
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
