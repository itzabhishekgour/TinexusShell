// ============================================================================
// LockWindow.qml — Qt6 / QML Lock Screen with LiquidGlass & PAM Integration
// Ref: Design Doc §6.1 (60px blur, 80% saturation, 70% brightness)
// ============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "../../common/qml"

Window {
    id: rootWindow
    width: 1920
    height: 1080
    color: "#0B0E14"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    Component.onCompleted: {
        passwordField.forceActiveFocus();
    }

    // ── Root Item for Global Focus and Key Handling ─────────────
    Item {
        id: rootItem
        anchors.fill: parent
        focus: true

        Keys.onReturnPressed: (event) => {
            if (typeof bridge !== "undefined") {
                bridge.authenticate(passwordField.text);
            }
            event.accepted = true;
        }

        Keys.onEnterPressed: (event) => {
            if (typeof bridge !== "undefined") {
                bridge.authenticate(passwordField.text);
            }
            event.accepted = true;
        }

        Keys.onEscapePressed: (event) => {
            passwordField.text = "";
            event.accepted = true;
        }

        // ── Full-Screen Frosted Glass Material ──────────────────────
        LiquidGlass {
            id: bgGlass
            anchors.fill: parent
            materialType: "fullscreen"
            blurAmount: 60.0
            tintColor: Qt.rgba(0.04, 0.06, 0.09, 0.70)
            cornerRadius: 0
        }

        // Clicking anywhere on screen refocuses the password input
        MouseArea {
            anchors.fill: parent
            z: 0
            onClicked: {
                passwordField.forceActiveFocus();
            }
        }

        // ── Unlock Fade Transition ──────────────────────────────────
        Behavior on opacity {
            NumberAnimation { duration: 350; easing.type: Easing.InOutQuad }
        }

        Connections {
            target: typeof bridge !== "undefined" ? bridge : null
            function onUnlockSuccess() {
                rootWindow.opacity = 0.0;
                exitTimer.start();
            }
            function onAuthFailedChanged() {
                if (bridge.authFailed) {
                    shakeAnim.start();
                    passwordField.text = "";
                }
            }
        }

        Timer {
            id: exitTimer
            interval: 350
            onTriggered: Qt.quit()
        }

        // ── Top: Clock & Date ───────────────────────────────────────
        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 120
            spacing: 8

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: typeof bridge !== "undefined" ? bridge.currentTime : "12:00 PM"
                color: "#FFFFFF"
                font.pixelSize: 64
                font.weight: Font.Light
                font.letterSpacing: -1
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: typeof bridge !== "undefined" ? bridge.currentDate : "Wednesday, September 10"
                color: Qt.rgba(1, 1, 1, 0.85)
                font.pixelSize: 18
                font.weight: Font.Normal
            }
        }

        // ── Center: User Profile & Password Input ───────────────────
        Item {
            id: loginBox
            width: 320
            height: 240
            anchors.centerIn: parent

            // Shake animation on authentication failure
            SequentialAnimation {
                id: shakeAnim
                NumberAnimation { target: loginBox; property: "x"; from: (rootWindow.width - loginBox.width)/2; to: (rootWindow.width - loginBox.width)/2 - 16; duration: 50; easing.type: Easing.InOutQuad }
                NumberAnimation { target: loginBox; property: "x"; to: (rootWindow.width - loginBox.width)/2 + 16; duration: 50; easing.type: Easing.InOutQuad }
                NumberAnimation { target: loginBox; property: "x"; to: (rootWindow.width - loginBox.width)/2 - 10; duration: 50; easing.type: Easing.InOutQuad }
                NumberAnimation { target: loginBox; property: "x"; to: (rootWindow.width - loginBox.width)/2 + 10; duration: 50; easing.type: Easing.InOutQuad }
                NumberAnimation { target: loginBox; property: "x"; to: (rootWindow.width - loginBox.width)/2; duration: 50; easing.type: Easing.InOutQuad }
                onFinished: {
                    if (typeof bridge !== "undefined") {
                        bridge.resetAuthFailed();
                    }
                }
            }

            Column {
                anchors.fill: parent
                spacing: 16

                // User Avatar
                Rectangle {
                    width: 76
                    height: 76
                    radius: 38
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Qt.rgba(1, 1, 1, 0.15)
                    border.color: Qt.rgba(1, 1, 1, 0.30)
                    border.width: 1.5

                    Text {
                        anchors.centerIn: parent
                        text: (typeof bridge !== "undefined" && bridge.username.length > 0)
                              ? bridge.username.charAt(0).toUpperCase() : "U"
                        color: "#FFFFFF"
                        font.pixelSize: 32
                        font.bold: true
                    }
                }

                // Username
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: typeof bridge !== "undefined" ? bridge.username : "User"
                    color: "#FFFFFF"
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                }

                // Password Capsule Input
                Rectangle {
                    id: passInputCapsule
                    width: 260
                    height: 38
                    radius: 19
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: Qt.rgba(0, 0, 0, 0.40)
                    border.color: (typeof bridge !== "undefined" && bridge.authFailed)
                                  ? "#EF4444" : (passwordField.activeFocus ? "#3B82F6" : Qt.rgba(1, 1, 1, 0.20))
                    border.width: 1.5

                    TextInput {
                        id: passwordField
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.right: submitBtn.left
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        echoMode: TextInput.Password
                        color: "#FFFFFF"
                        font.pixelSize: 14
                        focus: true
                        Component.onCompleted: forceActiveFocus()

                        Keys.onReturnPressed: (event) => {
                            if (typeof bridge !== "undefined") {
                                bridge.authenticate(passwordField.text);
                            }
                            event.accepted = true;
                        }

                        Keys.onEnterPressed: (event) => {
                            if (typeof bridge !== "undefined") {
                                bridge.authenticate(passwordField.text);
                            }
                            event.accepted = true;
                        }

                        Text {
                            anchors.fill: parent
                            text: "Press Enter to unlock"
                            color: Qt.rgba(1, 1, 1, 0.40)
                            font.pixelSize: 14
                            visible: !passwordField.text && !passwordField.activeFocus
                        }

                        onAccepted: {
                            if (typeof bridge !== "undefined") {
                                bridge.authenticate(passwordField.text);
                            }
                        }
                    }

                    // Submit arrow button
                    Rectangle {
                        id: submitBtn
                        width: 28
                        height: 28
                        radius: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        color: submitMouse.containsMouse ? "#2563EB" : "#3B82F6"

                        Text {
                            anchors.centerIn: parent
                            text: "→"
                            color: "#FFFFFF"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            id: submitMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (typeof bridge !== "undefined") {
                                    bridge.authenticate(passwordField.text);
                                }
                            }
                        }
                    }
                }

                // Error label
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: "Incorrect password. Try again."
                    color: "#EF4444"
                    font.pixelSize: 12
                    visible: typeof bridge !== "undefined" && bridge.authFailed
                }
            }
        }
    }
}
