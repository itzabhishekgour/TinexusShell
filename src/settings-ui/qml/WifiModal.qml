import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    visible: false
    color: Qt.rgba(0, 0, 0, 0.6) // Semi-transparent overlay scrim

    property string targetSsid: ""

    function openModal(ssid) {
        targetSsid = ssid
        passwordInput.text = ""
        visible = true
        console.log("[Qt6-Settings-UI] >>> ACTION: WifiModal.openModal() invoked for SSID: \"" + ssid + "\" — visible =", visible)
        passwordInput.forceActiveFocus()
    }

    function closeModal() {
        console.log("[Qt6-Settings-UI] >>> ACTION: WifiModal.closeModal() invoked — closing modal")
        visible = false
        targetSsid = ""
        console.log("[Qt6-Settings-UI] >>> ACTION: WifiModal closed — visible =", visible)
    }

    // Modal Card (macOS Dialog Sheet)
    Rectangle {
        anchors.centerIn: parent
        width: 380
        height: 230
        radius: 14
        color: "#28282a"
        border.color: "#20ffffff"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                spacing: 12
                MacSquircleIcon {
                    iconSize: 34
                    glyphSize: 18
                    iconColor: "#0A84FF"
                    iconText: "📶"
                }

                ColumnLayout {
                    spacing: 2
                    Text {
                        text: "Join Wi-Fi Network"
                        color: "#f5f5f7"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "Enter password for \"" + root.targetSsid + "\""
                        color: "#86868b"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            // Password Field
            Rectangle {
                Layout.fillWidth: true
                height: 32
                radius: 6
                color: "#1c1c1e"
                border.color: passwordInput.activeFocus ? "#0A84FF" : "#1fffffff"
                border.width: 1

                TextInput {
                    id: passwordInput
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 12
                    color: "#f5f5f7"
                    echoMode: showPasswordCheck.checked ? TextInput.Normal : TextInput.Password
                    clip: true
                    onAccepted: {
                        if (text.length > 0) {
                            bridge.connectWifi(root.targetSsid, text)
                            root.closeModal()
                        }
                    }

                    Text {
                        anchors.fill: parent
                        verticalAlignment: Text.AlignVCenter
                        text: "Password"
                        color: "#86868b"
                        font.pixelSize: 12
                        visible: !passwordInput.text && !passwordInput.activeFocus
                    }
                }
            }

            RowLayout {
                spacing: 8
                Rectangle {
                    width: 14
                    height: 14
                    radius: 3
                    color: showPasswordCheck.checked ? "#007aff" : "#343438"
                    border.color: "#24ffffff"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        color: "#ffffff"
                        font.pixelSize: 10
                        font.bold: true
                        visible: showPasswordCheck.checked
                    }

                    MouseArea {
                        id: showPasswordCheck
                        property bool checked: false
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: checked = !checked
                    }
                }

                Text {
                    text: "Show password"
                    color: "#d1d1d6"
                    font.pixelSize: 12
                }
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }

                MacButton {
                    text: "Cancel"
                    onClicked: root.closeModal()
                }

                MacButton {
                    text: "Join"
                    primary: true
                    enabled: passwordInput.text.length > 0
                    onClicked: {
                        bridge.connectWifi(root.targetSsid, passwordInput.text)
                        root.closeModal()
                    }
                }
            }
        }
    }
}
