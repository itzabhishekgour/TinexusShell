import QtQuick 2.15
import QtQuick.Controls 2.15

ListView {
    id: resultListView
    clip: true
    spacing: 4

    delegate: ResultItem {
        width: resultListView.width
        height: 52
    }
}
