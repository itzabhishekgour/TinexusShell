import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
    id: root
    clip: true
    contentWidth: availableWidth

    property string passwordTargetUser: ""

    ColumnLayout {
        width: Math.min(parent.width - 48, 680)
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 14

        Item { height: 4 }

        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "LOCAL ACCOUNTS"
                color: "#86868b"
                font.pixelSize: 11
                font.weight: Font.Medium
                Layout.leftMargin: 6
                Layout.fillWidth: true
            }
            MacButton {
                text: "Add Account..."
                primary: true
                onClicked: {
                    newUsernameInput.text = ""
                    newFullNameInput.text = ""
                    newPasswordInput.text = ""
                    addUserModal.visible = true
                }
            }
        }

        MacCard {
            Layout.fillWidth: true
            implicitHeight: usersCol.implicitHeight

            ColumnLayout {
                id: usersCol
                anchors.fill: parent
                spacing: 0

                Repeater {
                    model: bridge.systemUsers
                    delegate: MacSettingRow {
                        title: modelData.realName + (modelData.isCurrent ? " (Current)" : "")
                        subtitle: "@" + modelData.username + "  •  " + (modelData.isAdmin ? "Administrator" : "Standard User") + "  •  UID " + modelData.uid
                        showSeparator: index < bridge.systemUsers.length - 1

                        RowLayout {
                            spacing: 6

                            MacButton {
                                text: "Change Password..."
                                onClicked: {
                                    root.passwordTargetUser = modelData.username
                                    pwInput.text = ""
                                    changePwModal.visible = true
                                }
                            }

                            MacButton {
                                visible: !modelData.isCurrent
                                text: "Delete"
                                destructive: true
                                onClicked: bridge.deleteUser(modelData.username)
                            }
                        }
                    }
                }
            }
        }

        // ── Inline Add Account Dialog Card ────────────────────────────────
        MacCard {
            id: addUserModal
            visible: false
            Layout.fillWidth: true
            implicitHeight: addCol.implicitHeight + 28

            ColumnLayout {
                id: addCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    text: "Create New Linux Account"
                    color: "#f5f5f7"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: newFullNameInput
                    Layout.fillWidth: true
                    placeholderText: "Full Name (e.g. Alex Morgan)"
                    color: "#f5f5f7"
                    background: Rectangle { color: "#15161c"; radius: 6; border.color: "#22ffffff" }
                }
                TextField {
                    id: newUsernameInput
                    Layout.fillWidth: true
                    placeholderText: "Account Name (lowercase, e.g. alex)"
                    color: "#f5f5f7"
                    background: Rectangle { color: "#15161c"; radius: 6; border.color: "#22ffffff" }
                }
                TextField {
                    id: newPasswordInput
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: "Password (minimum 4 characters)"
                    color: "#f5f5f7"
                    background: Rectangle { color: "#15161c"; radius: 6; border.color: "#22ffffff" }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Text {
                        text: "Allow user to administer this computer (sudo)"
                        color: "#e5e5ea"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }
                    MacSwitch { id: newAdminSwitch; checked: false }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8
                    MacButton {
                        text: "Cancel"
                        onClicked: addUserModal.visible = false
                    }
                    MacButton {
                        text: "Create Account"
                        primary: true
                        onClicked: {
                            bridge.createUser(newUsernameInput.text, newFullNameInput.text, newPasswordInput.text, newAdminSwitch.checked)
                            addUserModal.visible = false
                        }
                    }
                }
            }
        }

        // ── Inline Change Password Card ───────────────────────────────────
        MacCard {
            id: changePwModal
            visible: false
            Layout.fillWidth: true
            implicitHeight: pwCol.implicitHeight + 28

            ColumnLayout {
                id: pwCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text {
                    text: "Change Password for @" + root.passwordTargetUser
                    color: "#f5f5f7"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                TextField {
                    id: pwInput
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: "New Password (minimum 4 characters)"
                    color: "#f5f5f7"
                    background: Rectangle { color: "#15161c"; radius: 6; border.color: "#22ffffff" }
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: 8
                    MacButton {
                        text: "Cancel"
                        onClicked: changePwModal.visible = false
                    }
                    MacButton {
                        text: "Save Password"
                        primary: true
                        onClicked: {
                            bridge.changeUserPassword(root.passwordTargetUser, pwInput.text)
                            changePwModal.visible = false
                        }
                    }
                }
            }
        }

        Item { height: 10 }
    }
}
