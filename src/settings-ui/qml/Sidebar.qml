import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: 240
    color: Qt.rgba(0.08, 0.09, 0.13, 0.72) // Frosted LiquidGlass sidebar backed by compositor Dual-Kawase blur

    // Top 1px specular glass sheen
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Qt.rgba(1, 1, 1, 0.14)
    }

    // Subtle right hairline separator
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Qt.rgba(1, 1, 1, 0.12)
    }

    property string searchText: ""

    property var navItems: [
        { name: "Wi-Fi",              papirusIcon: "network-wireless",            page: 3 },
        { name: "Bluetooth",          papirusIcon: "bluetooth",                   page: 8 },
        { name: "Sound",              papirusIcon: "preferences-desktop-sound",   page: 1 },
        { name: "Appearance",         papirusIcon: "preferences-desktop-theme",   page: 2 },
        { name: "Displays",           papirusIcon: "preferences-desktop-display", page: 0 },
        { name: "Battery & Power",    papirusIcon: "battery-full-charged",        page: 4 },
        { name: "Users & Accounts",   papirusIcon: "system-users",                page: 9 },
        { name: "Keyboard",           papirusIcon: "preferences-desktop-keyboard",page: 5 },
        { name: "Privacy & Security", papirusIcon: "preferences-system-privacy",  page: 6 },
        { name: "General",            papirusIcon: "preferences-system",          page: 7 }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 14
        anchors.bottomMargin: 12
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 10

        // ── Top Header: Traffic Lights & Drag Area ───────────────────────
        Item {
            Layout.fillWidth: true
            height: 24
            Layout.leftMargin: 4
            Layout.bottomMargin: 2

            MouseArea {
                anchors.fill: parent
                onPressed: {
                    var win = root.Window.window
                    if (win && typeof win.startSystemMove === "function") {
                        win.startSystemMove()
                    }
                }
            }

            RowLayout {
                anchors.fill: parent
                spacing: 8

                MacTrafficLights {
                    id: trafficLights
                    targetWindow: root.Window.window
                }

                Item { Layout.fillWidth: true }
            }
        }

        // ── Search Bar (macOS Spotlight-style Search) ────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 30
            radius: 8
            color: Qt.rgba(0, 0, 0, 0.30)
            border.color: searchInput.activeFocus ? (typeof bridge !== "undefined" ? bridge.accentColor : "#0A84FF") : Qt.rgba(1, 1, 1, 0.12)
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Image {
                    Layout.preferredWidth: 14
                    Layout.preferredHeight: 14
                    Layout.alignment: Qt.AlignVCenter
                    sourceSize.width: 14
                    sourceSize.height: 14
                    source: "image://tinexus/edit-find"
                    fillMode: Image.PreserveAspectFit
                    opacity: searchInput.activeFocus ? 1.0 : 0.72
                }

                TextInput {
                    id: searchInput
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 12
                    color: "#f5f5f7"
                    selectByMouse: true
                    clip: true
                    onTextChanged: {
                        if (typeof bridge !== "undefined") {
                            bridge.setSearchQuery(text)
                        }
                        root.searchText = text
                    }
                    Keys.onEscapePressed: {
                        text = ""
                        if (typeof bridge !== "undefined") bridge.clearSearch()
                        root.searchText = ""
                    }
                    Keys.onReturnPressed: {
                        if (typeof bridge !== "undefined" && bridge.searchResults.length > 0) {
                            bridge.selectPage(bridge.searchResults[0].page)
                        }
                    }

                    Text {
                        anchors.fill: parent
                        text: "Search Settings..."
                        color: "#86868b"
                        font.pixelSize: 12
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        visible: !searchInput.text && !searchInput.activeFocus
                    }
                }

                Text {
                    text: "×"
                    color: "#86868b"
                    font.pixelSize: 13
                    font.bold: true
                    visible: searchInput.text.length > 0

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            searchInput.text = ""
                            root.searchText = ""
                            if (typeof bridge !== "undefined") bridge.clearSearch()
                        }
                    }
                }
            }
        }

        // ── Real User Profile Card ───────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 48
            radius: 8
            color: profileArea.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 9

                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: typeof bridge !== "undefined" ? bridge.accentColor : "#0A84FF" }
                        GradientStop { position: 1.0; color: "#0062CC" }
                    }
                    border.color: Qt.rgba(1, 1, 1, 0.25)
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: typeof bridge !== "undefined" ? bridge.userInitials : "TX"
                        color: "#FFFFFF"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }

                ColumnLayout {
                    spacing: 1
                    Layout.fillWidth: true

                    Text {
                        text: typeof bridge !== "undefined" ? bridge.currentUserRealName : "Tinexus User"
                        color: "#f5f5f7"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "Local Account (" + (typeof bridge !== "undefined" ? bridge.currentUserName : "tinexus") + ")"
                        color: "#9898a0"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                Rectangle {
                    width: 16
                    height: 16
                    radius: 8
                    color: "#ff3b30"
                    visible: typeof bridge !== "undefined" && bridge.unverifiedAppsCount > 0

                    Text {
                        anchors.centerIn: parent
                        text: typeof bridge !== "undefined" ? bridge.unverifiedAppsCount : ""
                        color: "#ffffff"
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }

            MouseArea {
                id: profileArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: bridge.selectPage(9) // Users & Accounts
            }
        }

        // Hairline Divider
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Qt.rgba(1, 1, 1, 0.10)
        }

        // ── Navigation Categories List with Papirus Icons ────────────────
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: 3

                Text {
                    text: root.searchText.trim() ? "MATCHING CATEGORIES" : ""
                    color: typeof bridge !== "undefined" ? bridge.accentColor : "#0A84FF"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    visible: root.searchText.trim().length > 0
                    Layout.leftMargin: 6
                    Layout.topMargin: 2
                    Layout.bottomMargin: 2
                }

                Repeater {
                    model: {
                        if (typeof bridge !== "undefined") {
                            void(bridge.searchResults)
                        }
                        if (!root.searchText.trim()) return root.navItems
                        var filtered = []
                        for (var i = 0; i < root.navItems.length; ++i) {
                            var it = root.navItems[i]
                            if (typeof bridge !== "undefined" && bridge.isPageMatching(it.page)) {
                                filtered.push(it)
                            } else if (it.name.toLowerCase().indexOf(root.searchText.toLowerCase()) !== -1) {
                                filtered.push(it)
                            }
                        }
                        return filtered
                    }

                    delegate: Rectangle {
                        id: itemDelegate
                        Layout.fillWidth: true
                        height: 34
                        radius: 8

                        property bool isSelected: typeof bridge !== "undefined" && bridge.currentPage === modelData.page

                        color: isSelected
                               ? (typeof bridge !== "undefined" ? bridge.accentColor : "#0A84FF")
                               : (itemArea.pressed ? Qt.rgba(1, 1, 1, 0.14) : (itemArea.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent"))

                        Behavior on color {
                            ColorAnimation { duration: 120; easing.type: Easing.OutCubic }
                        }

                        border.color: isSelected ? Qt.rgba(1, 1, 1, 0.20) : "transparent"
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 9

                            Image {
                                width: 20
                                height: 20
                                source: "image://tinexus/" + modelData.papirusIcon
                                sourceSize: Qt.size(20, 20)
                                fillMode: Image.PreserveAspectFit
                            }

                            Text {
                                text: modelData.name
                                color: itemDelegate.isSelected ? "#ffffff" : "#ebebf0"
                                font.family: "Inter"
                                font.pixelSize: 13
                                font.weight: itemDelegate.isSelected ? Font.DemiBold : Font.Normal
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: itemArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: bridge.selectPage(modelData.page)
                        }
                    }
                }

                // Sub-Setting Matches Section when searching
                Text {
                    text: (root.searchText.trim() && typeof bridge !== "undefined" && bridge.searchResults.length > 0) ? "MATCHED SETTINGS" : ""
                    color: "#86868b"
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    visible: text.length > 0
                    Layout.leftMargin: 6
                    Layout.topMargin: 8
                    Layout.bottomMargin: 2
                }

                Repeater {
                    model: (root.searchText.trim() && typeof bridge !== "undefined") ? bridge.searchResults : []

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        height: 40
                        radius: 8
                        color: subArea.pressed ? Qt.rgba(1, 1, 1, 0.14) : (subArea.containsMouse ? Qt.rgba(1, 1, 1, 0.08) : "transparent")
                        Behavior on color { ColorAnimation { duration: 120; easing.type: Easing.OutCubic } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1

                                Text {
                                    text: modelData.title
                                    color: "#f5f5f7"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: modelData.subtitle
                                    color: "#9898a0"
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            Text {
                                text: "›"
                                color: "#86868b"
                                font.pixelSize: 14
                            }
                        }

                        MouseArea {
                            id: subArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: bridge.selectPage(modelData.page)
                        }
                    }
                }

                Text {
                    text: "No settings matching \"" + root.searchText + "\""
                    color: "#86868b"
                    font.pixelSize: 11
                    visible: root.searchText.trim().length > 0 && typeof bridge !== "undefined" && bridge.searchResults.length === 0
                    Layout.leftMargin: 10
                    Layout.topMargin: 12
                }
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "Tinexus OS v" + bridge.platformVersion
            color: "#7a7a82"
            font.pixelSize: 11
        }
    }
}
