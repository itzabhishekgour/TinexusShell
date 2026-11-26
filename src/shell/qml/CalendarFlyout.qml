// ============================================================================
// CalendarFlyout.qml — Redesigned 7x6 Interactive Calendar & Live Clock
// ============================================================================
import QtQuick
import QtQuick.Layouts
import "../../common/qml"

Item {
    id: root
    readonly property int shadowPad: 18
    readonly property int cardWidth: 284
    readonly property int cardHeight: 334

    width: cardWidth + shadowPad * 2
    height: cardHeight + shadowPad * 2

    property int monthOffset: 0
    property int selectedDay: -1
    property var currentDate: new Date()
    property string liveClockText: ""
    property string liveSubDateText: ""

    Timer {
        interval: 1000
        running: root.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: {
            var now = new Date()
            root.currentDate = now
            var hours = now.getHours()
            var minutes = now.getMinutes()
            var seconds = now.getSeconds()
            var ampm = hours >= 12 ? "PM" : "AM"
            hours = hours % 12
            hours = hours ? hours : 12
            var strH = hours < 10 ? ("0" + hours) : ("" + hours)
            var strM = minutes < 10 ? ("0" + minutes) : ("" + minutes)
            var strS = seconds < 10 ? ("0" + seconds) : ("" + seconds)
            root.liveClockText = strH + ":" + strM + ":" + strS + " " + ampm
            var days = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]
            var monthsShort = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
            root.liveSubDateText = days[now.getDay()] + ", " + monthsShort[now.getMonth()] + " " + now.getDate()
        }
    }

    readonly property var displayedDate: new Date(root.currentDate.getFullYear(), root.currentDate.getMonth() + root.monthOffset, 1)
    readonly property int displayedYear: displayedDate.getFullYear()
    readonly property int displayedMonth: displayedDate.getMonth()

    readonly property var monthNames: [
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    ]
    readonly property string headerTitle: monthNames[displayedMonth] + " " + displayedYear

    readonly property int totalDaysInMonth: new Date(displayedYear, displayedMonth + 1, 0).getDate()
    readonly property int prevMonthDays: new Date(displayedYear, displayedMonth, 0).getDate()
    readonly property int firstDayOfWeek: new Date(displayedYear, displayedMonth, 1).getDay()
    readonly property bool isCurrentMonth: root.monthOffset === 0
    readonly property int todayMday: root.currentDate.getDate()

    // Full 42-slot grid model (previous overflow + current month + next overflow)
    readonly property var calendarCells: {
        var cells = []
        for (var p = firstDayOfWeek - 1; p >= 0; --p) {
            cells.push({ day: prevMonthDays - p, inMonth: false, today: false })
        }
        for (var d = 1; d <= totalDaysInMonth; ++d) {
            cells.push({ day: d, inMonth: true, today: (isCurrentMonth && d === todayMday) })
        }
        var rem = 42 - cells.length
        for (var n = 1; n <= rem; ++n) {
            cells.push({ day: n, inMonth: false, today: false })
        }
        return cells
    }

    Item {
        id: glassCard
        x: root.shadowPad
        y: root.shadowPad
        width: root.cardWidth
        height: root.cardHeight

        LiquidGlass {
            anchors.fill: parent
            materialType: "popover"
            cornerRadius: 14
            blurAmount: 28
            fluidOpacity: 0.68
            shadowEnabled: true
            topSpecularOpacity: 0.14
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 8

            // ── Header: Month Year + Today Chip + < > Navigation ─────
            RowLayout {
                Layout.fillWidth: true
                height: 28
                spacing: 6

                Text {
                    text: root.headerTitle
                    color: "#FFFFFF"
                    font.family: "Inter"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Layout.fillWidth: true
                }

                // Quick jump back to current month
                Rectangle {
                    visible: root.monthOffset !== 0
                    Layout.preferredWidth: todayTxt.implicitWidth + 12
                    Layout.preferredHeight: 22
                    radius: 11
                    color: todayMouse.containsMouse ? Qt.rgba(10/255.0, 132/255.0, 255/255.0, 0.35) : Qt.rgba(10/255.0, 132/255.0, 255/255.0, 0.22)
                    border.color: Qt.rgba(10/255.0, 132/255.0, 255/255.0, 0.45)

                    Text {
                        id: todayTxt
                        anchors.centerIn: parent
                        text: "Today"
                        color: "#64D2FF"
                        font.family: "Inter"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }

                    MouseArea {
                        id: todayMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.monthOffset = 0
                            root.selectedDay = -1
                        }
                    }
                }

                Rectangle {
                    width: 26
                    height: 26
                    radius: 7
                    color: prevMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.05)

                    Image {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/go-previous-symbolic?color=#FFFFFF"
                    }

                    MouseArea {
                        id: prevMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.monthOffset--
                            root.selectedDay = -1
                        }
                    }
                }

                Rectangle {
                    width: 26
                    height: 26
                    radius: 7
                    color: nextMouse.containsMouse ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.05)

                    Image {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        fillMode: Image.PreserveAspectFit
                        source: "image://icon/go-next-symbolic?color=#FFFFFF"
                    }

                    MouseArea {
                        id: nextMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.monthOffset++
                            root.selectedDay = -1
                        }
                    }
                }
            }

            // ── Exact Integer 7-Column Weekday Strip (7 x 36 = 252) ───
            Grid {
                columns: 7
                width: 252
                spacing: 0
                Layout.alignment: Qt.AlignHCenter

                Repeater {
                    model: ["Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"]
                    delegate: Text {
                        width: 36
                        height: 20
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: modelData
                        color: Qt.rgba(1, 1, 1, 0.45)
                        font.family: "Inter"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // ── Integer 7x6 Date Cells ────────────────────────────────
            Grid {
                columns: 7
                width: 252
                spacing: 0
                Layout.alignment: Qt.AlignHCenter

                Repeater {
                    model: root.calendarCells
                    delegate: Item {
                        width: 36
                        height: 30

                        readonly property bool isToday: modelData.today
                        readonly property bool isSelected: modelData.inMonth && root.selectedDay === modelData.day && !isToday

                        Rectangle {
                            anchors.centerIn: parent
                            width: 28
                            height: 28
                            radius: 14
                            color: isToday
                                   ? "#0A84FF"
                                   : (isSelected
                                      ? Qt.rgba(56 / 255.0, 189 / 255.0, 248 / 255.0, 0.25)
                                      : (cellMouse.containsMouse && modelData.inMonth ? Qt.rgba(1, 1, 1, 0.14) : "transparent"))
                            border.color: isToday
                                          ? Qt.rgba(1, 1, 1, 0.35)
                                          : (isSelected ? "#38BDF8" : "transparent")
                            border.width: (isToday || isSelected) ? 1 : 0
                        }

                        Text {
                            anchors.centerIn: parent
                            text: modelData.day.toString()
                            color: isToday
                                   ? "#FFFFFF"
                                   : (modelData.inMonth ? Qt.rgba(1, 1, 1, 0.90) : Qt.rgba(1, 1, 1, 0.26))
                            font.family: "Inter"
                            font.pixelSize: 12
                            font.weight: isToday ? Font.Bold : (modelData.inMonth ? Font.Medium : Font.Normal)
                        }

                        MouseArea {
                            id: cellMouse
                            anchors.fill: parent
                            hoverEnabled: modelData.inMonth
                            enabled: modelData.inMonth
                            cursorShape: modelData.inMonth ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: root.selectedDay = modelData.day
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(1, 1, 1, 0.09) }

            // ── Bottom Live Clock Strip ───────────────────────────────
            RowLayout {
                Layout.fillWidth: true
                height: 20

                Text {
                    text: root.liveSubDateText
                    color: Qt.rgba(1, 1, 1, 0.55)
                    font.family: "Inter"
                    font.pixelSize: 11
                    Layout.fillWidth: true
                }

                Text {
                    text: root.liveClockText
                    color: "#64D2FF"
                    font.family: "Inter"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
            }
        }
    }
}
