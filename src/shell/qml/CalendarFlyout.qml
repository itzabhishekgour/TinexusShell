// ============================================================================
// CalendarFlyout.qml — Interactive Calendar & Live Clock with LiquidGlass
// Ref: TxUI CalendarFlyoutWidget.cpp:36-150
// ============================================================================
import QtQuick
import "../../common/qml"

Item {
    id: root
    width: 300
    height: 310

    property int monthOffset: 0
    property int selectedDay: -1
    property var currentDate: new Date()
    property string liveClockText: ""

    Timer {
        interval: 1000
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date();
            root.currentDate = now;
            var hours = now.getHours();
            var minutes = now.getMinutes();
            var seconds = now.getSeconds();
            var ampm = hours >= 12 ? "PM" : "AM";
            hours = hours % 12;
            hours = hours ? hours : 12; // 0 should be 12
            var strH = hours < 10 ? "0" + hours : hours;
            var strM = minutes < 10 ? "0" + minutes : minutes;
            var strS = seconds < 10 ? "0" + seconds : seconds;
            root.liveClockText = "Live Clock: " + strH + ":" + strM + ":" + strS + " " + ampm;
        }
    }

    readonly property var displayedDate: {
        var d = new Date(root.currentDate.getFullYear(), root.currentDate.getMonth() + root.monthOffset, 1);
        return d;
    }

    readonly property int displayedYear: displayedDate.getFullYear()
    readonly property int displayedMonth: displayedDate.getMonth() // 0-indexed

    readonly property var monthNames: [
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    ]

    readonly property string headerTitle: monthNames[displayedMonth] + " " + displayedYear

    // Days in month: day 0 of next month is last day of current month
    readonly property int totalDaysInMonth: new Date(displayedYear, displayedMonth + 1, 0).getDate()

    // Day of week of 1st day (0 = Sunday, 1 = Monday, ..., 6 = Saturday)
    readonly property int firstDayOfWeek: new Date(displayedYear, displayedMonth, 1).getDay()

    readonly property bool isCurrentMonth: root.monthOffset === 0
    readonly property int todayMday: root.currentDate.getDate()

    LiquidGlass {
        id: bg
        anchors.fill: parent
        materialType: "popover"
        cornerRadius: 14
        blurAmount: 24
        fluidOpacity: 0.90
    }

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // ── Header: Month & Year + Prev/Next Navigation ─────────────
        Row {
            width: parent.width
            height: 28

            Rectangle {
                width: 28
                height: 28
                radius: 6
                color: prevMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "◀"
                    color: prevMouse.containsMouse ? "#FFFFFF" : "#64D2FF"
                    font.pixelSize: 11
                }

                MouseArea {
                    id: prevMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.monthOffset--
                }
            }

            Item {
                width: parent.width - 56
                height: parent.height

                Text {
                    anchors.centerIn: parent
                    text: root.headerTitle
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }
            }

            Rectangle {
                width: 28
                height: 28
                radius: 6
                color: nextMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    anchors.centerIn: parent
                    text: "▶"
                    color: nextMouse.containsMouse ? "#FFFFFF" : "#64D2FF"
                    font.pixelSize: 11
                }

                MouseArea {
                    id: nextMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.monthOffset++
                }
            }
        }

        // ── Days of Week Header ─────────────────────────────────────
        Grid {
            columns: 7
            width: parent.width
            spacing: 0

            Repeater {
                model: ["SU", "MO", "TU", "WE", "TH", "FR", "SA"]
                delegate: Text {
                    width: (root.width - 28) / 7.0
                    height: 18
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: modelData
                    color: Qt.rgba(1, 1, 1, 0.45)
                    font.pixelSize: 10
                    font.bold: true
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.10)
        }

        // ── Calendar Day Cells ──────────────────────────────────────
        Grid {
            columns: 7
            width: parent.width
            rowSpacing: 2
            columnSpacing: 0

            // Leading blank days for offset
            Repeater {
                model: root.firstDayOfWeek
                delegate: Item {
                    width: (root.width - 28) / 7.0
                    height: 24
                }
            }

            // Days of the month (1 .. totalDaysInMonth)
            Repeater {
                model: root.totalDaysInMonth
                delegate: Item {
                    id: dayCell
                    readonly property int dayNum: index + 1
                    readonly property bool isToday: root.isCurrentMonth && dayNum === root.todayMday
                    readonly property bool isSelected: root.selectedDay === dayNum

                    width: (root.width - 28) / 7.0
                    height: 24

                    Rectangle {
                        anchors.centerIn: parent
                        width: 22
                        height: 22
                        radius: 11
                        color: isToday ? "#0A84FF" : (isSelected ? "#30D158" : (cellMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.15) : "transparent"))
                    }

                    Text {
                        anchors.centerIn: parent
                        text: dayCell.dayNum.toString()
                        color: isToday || isSelected ? "#FFFFFF" : (cellMouse.containsMouse ? "#FFFFFF" : Qt.rgba(1, 1, 1, 0.85))
                        font.pixelSize: 11
                        font.bold: isToday || isSelected
                    }

                    MouseArea {
                        id: cellMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.selectedDay = dayCell.dayNum;
                        }
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: 4
        }

        Rectangle {
            width: parent.width
            height: 1
            color: Qt.rgba(1, 1, 1, 0.10)
        }

        // ── Live Clock Footer ───────────────────────────────────────
        Row {
            width: parent.width
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter

            Text {
                text: "🕒"
                font.pixelSize: 11
                color: "#64D2FF"
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: root.liveClockText
                color: "#64D2FF"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }
}
