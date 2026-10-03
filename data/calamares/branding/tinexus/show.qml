import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: presentation
    anchors.fill: parent

    Rectangle {
        anchors.fill: parent
        color: "#13131A"

        SwipeView {
            id: view
            anchors.fill: parent
            currentIndex: indicator.currentIndex

            Item {
                id: slide1
                Column {
                    anchors.centerIn: parent
                    spacing: 16
                    Image {
                        source: "tinexus-logo.png"
                        width: 96
                        height: 96
                        anchors.horizontalCenter: parent.horizontalCenter
                        fillMode: Image.PreserveAspectFit
                    }
                    Text {
                        text: "Welcome to Tinexus OS"
                        color: "#F0F0F8"
                        font.pixelSize: 22
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "A Wayland-native desktop built for elegance, responsiveness, and control."
                        color: "#9090A8"
                        font.pixelSize: 13
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            Item {
                id: slide2
                Column {
                    anchors.centerIn: parent
                    spacing: 16
                    Text {
                        text: "LiquidGlass & Aura Notch"
                        color: "#6B8CEF"
                        font.pixelSize: 20
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "Specular materials with 40px backdrop blur, fluid physics, and seamless system status."
                        color: "#F0F0F8"
                        font.pixelSize: 13
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            Item {
                id: slide3
                Column {
                    anchors.centerIn: parent
                    spacing: 16
                    Text {
                        text: "Command Palette Launcher"
                        color: "#6BEFAC"
                        font.pixelSize: 20
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "Press Ctrl+K anywhere to launch apps, search files, perform calculations, and control settings."
                        color: "#F0F0F8"
                        font.pixelSize: 13
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            Item {
                id: slide4
                Column {
                    anchors.centerIn: parent
                    spacing: 16
                    Text {
                        text: "Pure C++20 Core & Modern Supervisor"
                        color: "#EFC86B"
                        font.pixelSize: 20
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    Text {
                        text: "Engineered from scratch for sub-millisecond IPC, zero bloat, and total privacy."
                        color: "#F0F0F8"
                        font.pixelSize: 13
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }
        }

        PageIndicator {
            id: indicator
            count: view.count
            currentIndex: view.currentIndex
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 16
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Timer {
            interval: 5000
            running: true
            repeat: true
            onTriggered: {
                view.currentIndex = (view.currentIndex + 1) % view.count;
            }
        }
    }
}
