import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    visible: true
    width: 480
    height: 360
    title: "Tinexus Qt6 xdg_popup Test Harness"
    color: "#0f172a"

    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "Qt6 Wayland xdg_popup Test"
            color: "#f8fafc"
            font.pixelSize: 18
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            width: 300
        }

        Button {
            id: testButton
            text: "Click Count: " + clickCount
            property int clickCount: 0
            width: 300
            height: 44
            onClicked: {
                clickCount++
                statusText.text = "Button clicked " + clickCount + " times"
                console.log("[Qt6-Popup-Test] Button clicked, count =", clickCount)
            }
        }

        ComboBox {
            id: testComboBox
            width: 300
            height: 44
            model: [
                "Profile 1: Balanced",
                "Profile 2: Performance",
                "Profile 3: Power Saver",
                "Profile 4: Custom"
            ]
            onActivated: (index) => {
                statusText.text = "Selected: " + currentText
                console.log("[Qt6-Popup-Test] ComboBox activated, currentText =", currentText)
            }
            popup.onOpened: {
                console.log("[Qt6-Popup-Test] QQC2 ComboBox.popup EVENT: opened, visible =", popup.visible)
            }
            popup.onClosed: {
                console.log("[Qt6-Popup-Test] QQC2 ComboBox.popup EVENT: closed, visible =", popup.visible)
            }
        }

        Text {
            id: statusText
            text: "Ready — Testing xdg_popup"
            color: "#94a3b8"
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            width: 300
        }
    }

    Window {
        id: popupWindow1
        objectName: "popupWindow1"
        transientParent: root
        flags: Qt.ToolTip
        width: 260
        height: 140
        color: "#1e293b"

        Column {
            anchors.centerIn: parent
            spacing: 10
            Text {
                text: "Popup Window #1 (Alpha)"
                color: "#38bdf8"
                font.bold: true
                font.pixelSize: 14
            }
            Text {
                text: "First independent xdg_popup surface"
                color: "#94a3b8"
                font.pixelSize: 12
            }
        }
    }

    Window {
        id: popupWindow2
        objectName: "popupWindow2"
        transientParent: root
        flags: Qt.ToolTip
        width: 280
        height: 150
        color: "#334155"

        Column {
            anchors.centerIn: parent
            spacing: 10
            Text {
                text: "Popup Window #2 (Beta)"
                color: "#4ade80"
                font.bold: true
                font.pixelSize: 14
            }
            Text {
                text: "Second independent xdg_popup surface"
                color: "#94a3b8"
                font.pixelSize: 12
            }
        }
    }

    Timer {
        id: autoTestTimer
        interval: 800
        running: false
        repeat: true
        property int step: 0

        onTriggered: {
            step++
            if (step === 1) {
                console.log("[Qt6-Popup-Test] >>> STEP 1: Calling testComboBox.popup.open() (QQC2 ComboBox Dropdown)")
                testComboBox.popup.open()
            } else if (step === 2) {
                console.log("[Qt6-Popup-Test] >>> STEP 2: ComboBox popup state — visible =", testComboBox.popup.visible)
                console.log("[Qt6-Popup-Test] >>> STEP 2: Calling testComboBox.popup.close()")
                testComboBox.popup.close()
            } else if (step === 3) {
                console.log("[Qt6-Popup-Test] >>> STEP 3: ComboBox popup closed — visible =", testComboBox.popup.visible)
                console.log("[Qt6-Popup-Test] >>> STEP 3: Calling popupWindow1.show() (Native Window with Qt.ToolTip & transientParent)")
                popupWindow1.show()
            } else if (step === 4) {
                console.log("[Qt6-Popup-Test] >>> STEP 4: popupWindow1 state — visible =", popupWindow1.visible)
                console.log("[Qt6-Popup-Test] >>> STEP 4: Calling popupWindow1.hide()")
                popupWindow1.hide()
            } else if (step === 5) {
                console.log("[Qt6-Popup-Test] >>> STEP 5: popupWindow1 closed — visible =", popupWindow1.visible)
                console.log("[Qt6-Popup-Test] >>> STEP 5: Sequence complete. Exiting cleanly.")
                autoTestTimer.stop()
                Qt.quit()
            }
        }
    }

    Component.onCompleted: {
        if (Qt.application.arguments.indexOf("--auto-test") !== -1) {
            console.log("[Qt6-Popup-Test] --auto-test flag detected, starting sequence in 300ms")
            autoTestTimer.start()
        }
    }
}
