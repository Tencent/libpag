
import QtQuick 2.5
import QtQuick.Controls 2.1

TabButton{
    property var windowColor: Qt.rgba(20/255,20/255,30/255,1)
    property var underLineVisible: false

    id: tabButton
    width: 100
    height: parent.height
    background: Rectangle {
        border.color: windowColor
    }
    contentItem: Rectangle{
        color: windowColor
        anchors.fill: parent
        clip: true
        
        Text{
            id: titleText
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            font.pixelSize: 16
            font.family: "PingFang SC"
            text: tabButton.text
            color: "#FFFFFF"
            elide: Text.ElideMiddle
        }

        Rectangle{
            id: undlerLine
            anchors.bottom: parent.bottom
            width: parent.width
            height: 2
            color:"#1982EB"
            visible: tabButton.underLineVisible
        }
    }

    Component.onCompleted:{
        resetTitleWidth()
    }

    function resetTitleWidth() {
        tabButton.width = titleText.width + 15
    }
}