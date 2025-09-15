import QtQuick
import QtQuick.Controls

TabButton {
    id: tabButton

    height: 36
    font.pixelSize: 13
    anchors.top: parent.top
    anchors.topMargin: 0

    background: Rectangle {
        color: tabButton.checked ? "#2D2D37" : "#20202A"
    }

    contentItem: Text {
        text: tabButton.text
        font: tabButton.font
        color: tabButton.checked ? "#FFFFFF" : "#9B9B9B"
        anchors.fill: parent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
