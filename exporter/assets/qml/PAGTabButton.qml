import QtQuick
import QtQuick.Controls

TabButton {
    id: tabButton
    height: parent.height
    anchors.top: parent.top
    contentItem: Rectangle {
        color: "#22222c"
        anchors.fill: parent
        clip: true

        Text {
            id: displayText
            text: tabButton.text
            font.pixelSize: 15
            font.family: "PingFang SC"
            elide: Text.ElideMiddle
            color: "#FFFFFF"
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Rectangle {
            id: undlerLine
            anchors.bottom: parent.bottom
            width: parent.width
            height: 2
            color: "#1982EB"
            visible: tabButton.checked
        }
    }
}
